#include "config.h"
#include <sdbusplus/bus.hpp>


#include "item_updater.hpp"
#include "hostimagetype.hpp"

#include "images.hpp"
#include "serialize.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Software/ExtendedVersion/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include "xyz/openbmc_project/Common/error.hpp"
#include "xyz/openbmc_project/Software/Image/error.hpp"

#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <boost/algorithm/string.hpp>


namespace phosphor
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace control = sdbusplus::xyz::openbmc_project::Control::server;

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Image::Error;
using namespace phosphor::software::image;
namespace fs = std::filesystem;
using NotAllowed = sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;


ItemUpdater::ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path)
    : ItemUpdaterInherit(bus, path.c_str(), false)
    , bus(bus)
    , helper(bus)
    , versionMatch(bus, MatchRules::interfacesAdded()
                        + MatchRules::path("/xyz/openbmc_project/software"),
                   std::bind(std::mem_fn(&ItemUpdater::createActivation),
                             this, std::placeholders::_1)
                  )
{
    setBMCInventoryPath();
    processBMCImage();
    restoreFieldModeStatus();
    emit_object_added();
}

void ItemUpdater::createActivation(sdbusplus::message::message& msg)
{
    using SVersion = server::Version;
    using VersionPurpose = SVersion::VersionPurpose;
    using VersionClass = phosphor::software::manager::Version;

    sdbusplus::message::object_path objPath;
    auto purpose = VersionPurpose::Unknown;
    std::string extendedVersion;
    std::string version;
    std::map<std::string, std::map<std::string, std::variant<std::string>>>
            interfaces;
    msg.read(objPath, interfaces);
    std::string path(std::move(objPath));
    std::string filePath;
    std::string topLevelFirmareHostObjectPath;

    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    auto value = SVersion::convertVersionPurposeFromString(
                                std::get<std::string>(property.second));
                    if (value == VersionPurpose::BMC
                            || value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
#ifdef HOST_FIRMWARE_UPGRADE
                    else if (value == VersionPurpose::Host)
                    {
                        purpose = value;
                        // save main image object path
                        topLevelFirmareHostObjectPath = path;
                    }
#endif
                }
                else if (property.first == "Version")
                {
                    version = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == EXTENDED_VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "ExtendedVersion")
                {
                    extendedVersion = std::get<std::string>(property.second);
                }
            }
        }
    }
    if (version.empty() || filePath.empty() ||
            purpose == VersionPurpose::Unknown)
    {
        return;
    }

    // Version id is the last item in the path
    auto pos = path.rfind("/");
    if (pos == std::string::npos)
    {
        log<level::ERR>("No version id found in object path",
                        entry("OBJPATH=%s", path.c_str()));
        return;
    }

    auto versionId = path.substr(pos + 1);

    if (activations.find(versionId) == activations.end())
    {
        // Determine the Activation state by processing the given image dir.
        auto activationState = server::Activation::Activations::Invalid;
        ItemUpdater::ActivationStatus result;
        if (purpose == VersionPurpose::BMC || purpose == VersionPurpose::System)
            result = ItemUpdater::validateSquashFSImage(filePath);
        else
            result = ItemUpdater::ActivationStatus::ready;

        AssociationList associations = {};

        if (result == ItemUpdater::ActivationStatus::ready)
        {
            activationState = server::Activation::Activations::Ready;
            // Create an association to the BMC inventory item
            associations.emplace_back(
                        std::make_tuple(ACTIVATION_FWD_ASSOCIATION,
                                        ACTIVATION_REV_ASSOCIATION,
                                        bmcInventoryPath));
        }

        activations.insert(std::make_pair(
                               versionId,
                               std::make_unique<Activation>(bus,
                                                            path,
                                                            *this,
                                                            versionId,
                                                            activationState,
                                                            associations)));

        auto versionPtr = std::make_unique<VersionClass>(
                  bus, path, version, purpose, extendedVersion, filePath,
                  std::bind(&ItemUpdater::erase, this, std::placeholders::_1));
        versionPtr->deleteObject =
            std::make_unique<phosphor::software::manager::Delete>(bus,
                                                                  path,
                                                                  *versionPtr);
        versions.insert(std::make_pair(versionId, std::move(versionPtr)));

#ifdef HOST_FIRMWARE_UPGRADE
        if (topLevelFirmareHostObjectPath.empty() == false)
        {
            createFirmwareObjectTree(versionId,
                                     topLevelFirmareHostObjectPath,
                                     filePath);
            topLevelFirmareHostObjectPath.clear();
        }
#endif

    }
    return;
}

void ItemUpdater::processBMCImage()
{
    using VersionClass = phosphor::software::manager::Version;

    // Check MEDIA_DIR and create if it does not exist
    try
    {
        if (!fs::is_directory(MEDIA_DIR))
        {
            fs::create_directory(MEDIA_DIR);
        }
    }
    catch (const fs::filesystem_error& e)
    {
        log<level::ERR>("Failed to prepare dir", entry("ERR=%s", e.what()));
        return;
    }

    // Read os-release from /etc/ to get the functional BMC version
    auto functionalVersion = VersionClass::getBMCVersion(OS_RELEASE_FILE);

    // Read os-release from folders under /media/ to get
    // BMC Software Versions.
    for (const auto& iter : fs::directory_iterator(MEDIA_DIR))
    {
        auto activationState = server::Activation::Activations::Active;
        static const auto BMC_RO_PREFIX_LEN = strlen(BMC_ROFS_PREFIX);

        // Check if the BMC_RO_PREFIXis the prefix of the iter.path
        if (0 ==
                iter.path().native().compare(0,
                                             BMC_RO_PREFIX_LEN,
                                             BMC_ROFS_PREFIX))
        {
            // Get the version to calculate the id
            fs::path releaseFile(OS_RELEASE_FILE);
            auto osRelease = iter.path() / releaseFile.relative_path();
            if (!fs::is_regular_file(osRelease))
            {
                log<level::ERR>(
                            "Failed to read osRelease",
                            entry("FILENAME=%s", osRelease.string().c_str()));

                // Try to get the version id from the mount directory name and
                // call to delete it as this version may be corrupted. Dynamic
                // volumes created by the UBI layout for example have the id in
                // the mount directory name. The worst that can happen is that
                // erase() is called with an non-existent id and returns.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                ItemUpdater::erase(id);

                continue;
            }
            auto version = VersionClass::getBMCVersion(osRelease);
            if (version.empty())
            {
                log<level::ERR>(
                            "Failed to read version from osRelease",
                            entry("FILENAME=%s", osRelease.string().c_str()));

                // Try to delete the version, same as above if the
                // OS_RELEASE_FILE does not exist.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                ItemUpdater::erase(id);

                continue;
            }

            auto id = VersionClass::getId(version);

            // Check if the id has already been added. This can happen if the
            // BMC partitions / devices were manually flashed with the same
            // image.
            if (versions.find(id) != versions.end())
            {
                continue;
            }

            auto purpose = server::Version::VersionPurpose::BMC;
            restorePurpose(id, purpose);

            // Read os-release from /etc/ to get the BMC extended version
            std::string extendedVersion =
                    VersionClass::getBMCExtendedVersion(osRelease);

            auto path = fs::path(SOFTWARE_OBJPATH) / id;

            // Create functional association if this is the functional
            // version
            if (version.compare(functionalVersion) == 0)
            {
                createFunctionalAssociation(path);
            }

            AssociationList associations = {};

            if (activationState == server::Activation::Activations::Active)
            {
                // Create an association to the BMC inventory item
                associations.emplace_back(std::make_tuple(
                                              ACTIVATION_FWD_ASSOCIATION,
                                              ACTIVATION_REV_ASSOCIATION,
                                              bmcInventoryPath));

                // Create an active association since this image is active
                createActiveAssociation(path);
            }

            // All updateable firmware components must expose the updateable
            // association.
            createUpdateableAssociation(path);

            // Create Version instance for this version.
            auto versionPtr = std::make_unique<VersionClass>(
                        bus, path, version, purpose, extendedVersion, "",
                        std::bind(&ItemUpdater::erase,
                                  this,
                                  std::placeholders::_1));
            auto isVersionFunctional = versionPtr->isFunctional();
            if (!isVersionFunctional)
            {
                versionPtr->deleteObject =
                        std::make_unique<phosphor::software::manager::Delete>(
                            bus, path, *versionPtr);
            }
            versions.insert(std::make_pair(id, std::move(versionPtr)));

            // Create Activation instance for this version.
            activations.insert(std::make_pair(
                                   id, std::make_unique<Activation>(
                                       bus, path, *this, id, activationState,
                                       associations)));

            // If Active, create RedundancyPriority instance for this
            // version.
            if (activationState == server::Activation::Activations::Active)
            {
                uint8_t priority = std::numeric_limits<uint8_t>::max();
                if (!restorePriority(id, priority))
                {
                    if (isVersionFunctional)
                    {
                        priority = 0;
                    }
                    else
                    {
                        log<level::ERR>("Unable to restore priority from file.",
                                        entry("VERSIONID=%s", id.c_str()));
                    }
                }
                activations.find(id)->second->redundancyPriority =
                        std::make_unique<RedundancyPriority>(
                          bus, path, *(activations.find(id)->second), priority,
                          false);
            }
        }
    }

    // If there are no bmc versions mounted under MEDIA_DIR, then read the
    // /etc/os-release and create rofs-<versionId> under MEDIA_DIR, then call
    // again processBMCImage() to create the D-Bus interface for it.
    if (activations.size() == 0)
    {
        auto version = VersionClass::getBMCVersion(OS_RELEASE_FILE);
        auto id = phosphor::software::manager::Version::getId(version);
        auto versionFileDir = BMC_ROFS_PREFIX + id + "/etc/";
        try
        {
            if (!fs::is_directory(versionFileDir))
            {
                fs::create_directories(versionFileDir);
            }
            auto versionFilePath = BMC_ROFS_PREFIX + id + OS_RELEASE_FILE;
            fs::create_directory_symlink(OS_RELEASE_FILE, versionFilePath);
            ItemUpdater::processBMCImage();
        }
        catch (const std::exception& e)
        {
            log<level::ERR>(e.what());
        }
    }

    mirrorUbootToAlt();
    return;
}

void ItemUpdater::erase(std::string entryId)
{
    // Find entry in versions map
    auto it = versions.find(entryId);
    if (it != versions.end())
    {
        if (it->second->isFunctional() && ACTIVE_BMC_MAX_ALLOWED > 1)
        {
            log<level::ERR>("Error: Version is currently running on the BMC. "
                            "Unable to remove.",
                            entry("VERSIONID=%s", entryId.c_str()));
            return;
        }
    }

    // First call resetUbootEnvVars() so that the BMC points to a valid image to
    // boot from. If resetUbootEnvVars() is called after the image is actually
    // deleted from the BMC flash, there'd be a time window where the BMC would
    // be pointing to a non-existent image to boot from.
    // Need to remove the entries from the activations map before that call so
    // that resetUbootEnvVars() doesn't use the version to be deleted.
    auto iteratorActivations = activations.find(entryId);
    if (iteratorActivations == activations.end())
    {
        log<level::ERR>("Error: Failed to find version in item updater "
                        "activations map. Unable to remove.",
                        entry("VERSIONID=%s", entryId.c_str()));
    }
    else
    {
        removeAssociations(iteratorActivations->second->path);
        this->activations.erase(entryId);
    }
    ItemUpdater::resetUbootEnvVars();

    if (it != versions.end())
    {
        // Delete ReadOnly partitions if it's not active
        removeReadOnlyPartition(entryId);
        removePersistDataDirectory(entryId);

        // Removing entry in versions map
        this->versions.erase(entryId);
    }
    else
    {
        // Delete ReadOnly partitions even if we can't find the version
        removeReadOnlyPartition(entryId);
        removePersistDataDirectory(entryId);

        log<level::ERR>("Error: Failed to find version in item updater "
                        "versions map. Unable to remove.",
                        entry("VERSIONID=%s", entryId.c_str()));
    }

    helper.clearEntry(entryId);

    return;
}

void ItemUpdater::deleteAll()
{
    std::vector<std::string> deletableVersions;

    for (const auto& versionIt : versions)
    {
        if (!versionIt.second->isFunctional())
        {
            deletableVersions.push_back(versionIt.first);
        }
    }

    for (const auto& deletableIt : deletableVersions)
    {
        ItemUpdater::erase(deletableIt);
    }

    helper.cleanup();
}

ItemUpdater::ActivationStatus
    ItemUpdater::validateSquashFSImage(const std::string& filePath)
{
    bool valid = true;

    // Record the images which are being updated
    // First check for the fullimage, then check for images with partitions
    imageUpdateList.push_back(bmcFullImages);
    valid = checkImage(filePath, imageUpdateList);
    if (!valid)
    {
        imageUpdateList.clear();
        imageUpdateList.assign(bmcImages.begin(), bmcImages.end());
        valid = checkImage(filePath, imageUpdateList);
        if (!valid)
        {
            log<level::ERR>("Failed to find the needed BMC images.");
            return ItemUpdater::ActivationStatus::invalid;
        }
    }

    return ItemUpdater::ActivationStatus::ready;
}

void ItemUpdater::savePriority(const std::string& versionId, uint8_t value)
{
    storePriority(versionId, value);
    helper.setEntry(versionId, value);
}

void ItemUpdater::freePriority(uint8_t value, const std::string& versionId)
{
    std::map<std::string, uint8_t> priorityMap;

    // Insert the requested version and priority, it may not exist yet.
    priorityMap.insert(std::make_pair(versionId, value));

    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            priorityMap.insert(std::make_pair(
                intf.first, intf.second->redundancyPriority.get()->priority()));
        }
    }

    // Lambda function to compare 2 priority values, use <= to allow duplicates
    typedef std::function<bool(std::pair<std::string, uint8_t>,
                               std::pair<std::string, uint8_t>)>
        cmpPriority;
    cmpPriority cmpPriorityFunc =
        [](std::pair<std::string, uint8_t> priority1,
           std::pair<std::string, uint8_t> priority2) {
            return priority1.second <= priority2.second;
        };

    // Sort versions by ascending priority
    std::set<std::pair<std::string, uint8_t>, cmpPriority> prioritySet(
        priorityMap.begin(), priorityMap.end(), cmpPriorityFunc);

    auto freePriorityValue = value;
    for (auto& element : prioritySet)
    {
        if (element.first == versionId)
        {
            continue;
        }
        if (element.second == freePriorityValue)
        {
            ++freePriorityValue;
            auto it = activations.find(element.first);
            it->second->redundancyPriority.get()->sdbusPriority(
                freePriorityValue);
        }
    }

    auto lowestVersion = prioritySet.begin()->first;
    if (value == prioritySet.begin()->second)
    {
        lowestVersion = versionId;
    }
    updateUbootEnvVars(lowestVersion);
}

void ItemUpdater::reset()
{
    helper.factoryReset();

    log<level::INFO>("BMC factory reset will take effect upon reboot.");
}

void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
    helper.removeVersion(versionId);
}

bool ItemUpdater::fieldModeEnabled(bool value)
{
    // enabling field mode is intended to be one way: false -> true
    if (value && !control::FieldMode::fieldModeEnabled())
    {
        control::FieldMode::fieldModeEnabled(value);

        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        method.append("obmc-flash-bmc-setenv@fieldmode\\x3dtrue.service",
                      "replace");
        bus.call_noreply(method);

        method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                     SYSTEMD_INTERFACE, "StopUnit");
        method.append("usr-local.mount", "replace");
        bus.call_noreply(method);

        std::vector<std::string> usrLocal = {"usr-local.mount"};

        method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                     SYSTEMD_INTERFACE, "MaskUnitFiles");
        method.append(usrLocal, false, true);
        bus.call_noreply(method);
    }
    else if (!value && control::FieldMode::fieldModeEnabled())
    {
        elog<NotAllowed>(xyz::openbmc_project::Common::NotAllowed::REASON(
            "FieldMode is not allowed to be cleared"));
    }

    return control::FieldMode::fieldModeEnabled();
}

void ItemUpdater::restoreFieldModeStatus()
{
    std::ifstream input("/dev/mtd/u-boot-env");
    std::string envVar;
    std::getline(input, envVar);

    if (envVar.find("fieldmode=true") != std::string::npos)
    {
        ItemUpdater::fieldModeEnabled(true);
    }
}

void ItemUpdater::setBMCInventoryPath()
{
    auto depth = 0;
    auto mapperCall = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                          MAPPER_INTERFACE, "GetSubTreePaths");

    mapperCall.append(INVENTORY_PATH);
    mapperCall.append(depth);
    std::vector<std::string> filter = {BMC_INVENTORY_INTERFACE};
    mapperCall.append(filter);

    try
    {
        auto response = bus.call(mapperCall);

        using ObjectPaths = std::vector<std::string>;
        ObjectPaths result;
        response.read(result);

        if (!result.empty())
        {
            bmcInventoryPath = result.front();
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("Error in mapper GetSubTreePath");
        return;
    }

    return;
}

void ItemUpdater::createActiveAssociation(const std::string& path)
{
    assocs.emplace_back(
        std::make_tuple(ACTIVE_FWD_ASSOCIATION, ACTIVE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::createFunctionalAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(FUNCTIONAL_FWD_ASSOCIATION,
                                        FUNCTIONAL_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::createUpdateableAssociation(const std::string& path)
{
    assocs.emplace_back(std::make_tuple(UPDATEABLE_FWD_ASSOCIATION,
                                        UPDATEABLE_REV_ASSOCIATION, path));
    associations(assocs);
}

void ItemUpdater::removeAssociations(const std::string& path)
{
    for (auto iter = assocs.begin(); iter != assocs.end();)
    {
        if ((std::get<2>(*iter)).compare(path) == 0)
        {
            iter = assocs.erase(iter);
            associations(assocs);
        }
        else
        {
            ++iter;
        }
    }
}

bool ItemUpdater::isLowestPriority(uint8_t value)
{
    for (const auto& intf : activations)
    {
        if (intf.second->redundancyPriority)
        {
            if (intf.second->redundancyPriority.get()->priority() < value)
            {
                return false;
            }
        }
    }
    return true;
}

void ItemUpdater::updateUbootEnvVars(const std::string& versionId)
{
    helper.updateUbootVersionId(versionId);
}

void ItemUpdater::resetUbootEnvVars()
{
    decltype(activations.begin()->second->redundancyPriority.get()->priority())
        lowestPriority = std::numeric_limits<uint8_t>::max();
    decltype(activations.begin()->second->versionId) lowestPriorityVersion;
    for (const auto& intf : activations)
    {
        if (!intf.second->redundancyPriority.get())
        {
            // Skip this version if the redundancyPriority is not initialized.
            continue;
        }

        if (intf.second->redundancyPriority.get()->priority() <= lowestPriority)
        {
            lowestPriority = intf.second->redundancyPriority.get()->priority();
            lowestPriorityVersion = intf.second->versionId;
        }
    }

    // Update the U-boot environment variable to point to the lowest priority
    updateUbootEnvVars(lowestPriorityVersion);
}

void ItemUpdater::freeSpace(Activation& caller)
{
    //  Versions with the highest priority in front
    std::priority_queue<std::pair<int, std::string>,
                        std::vector<std::pair<int, std::string>>,
                        std::less<std::pair<int, std::string>>>
        versionsPQ;

    std::size_t count = 0;
    for (const auto& iter : activations)
    {
        if ((iter.second.get()->activation() ==
             server::Activation::Activations::Active) ||
            (iter.second.get()->activation() ==
             server::Activation::Activations::Failed))
        {
            count++;
            // Don't put the functional version on the queue since we can't
            // remove the "running" BMC version.
            // If ACTIVE_BMC_MAX_ALLOWED <= 1, there is only one active BMC,
            // so remove functional version as well.
            // Don't delete the the Activation object that called this function.
            if ((versions.find(iter.second->versionId)
                     ->second->isFunctional() &&
                 ACTIVE_BMC_MAX_ALLOWED > 1) ||
                (iter.second->versionId == caller.versionId))
            {
                continue;
            }

            // Failed activations don't have priority, assign them a large value
            // for sorting purposes.
            auto priority = 999;
            if (iter.second.get()->activation() ==
                    server::Activation::Activations::Active &&
                iter.second->redundancyPriority)
            {
                priority = iter.second->redundancyPriority.get()->priority();
            }

            versionsPQ.push(std::make_pair(priority, iter.second->versionId));
        }
    }

    // If the number of BMC versions is over ACTIVE_BMC_MAX_ALLOWED -1,
    // remove the highest priority one(s).
    while ((count >= ACTIVE_BMC_MAX_ALLOWED) && (!versionsPQ.empty()))
    {
        erase(versionsPQ.top().second);
        versionsPQ.pop();
        count--;
    }
}

void ItemUpdater::mirrorUbootToAlt()
{
    helper.mirrorAlt();
}

bool ItemUpdater::checkImage(const std::string& filePath,
                             const std::vector<std::string>& imageList)
{
    bool valid = true;

    for (auto& bmcImage : imageList)
    {
        fs::path file(filePath);
        file /= bmcImage;
        std::ifstream efile(file.c_str());
        if (efile.good() != 1)
        {
            valid = false;
            break;
        }
    }

    return valid;
}

#ifdef HOST_FIRMWARE_UPGRADE
void
ItemUpdater::createFirmwareObjectTree(const std::string& versionId,
                                      const std::string& mainImageObjectPath,
                                      const std::string& imageDirPath)
{
    auto firmwarePath = mainImageObjectPath;
    // store the path objects related to image type
    //  (usually one if a image type can be detected)
    std::vector<std::string> imageTypesToCreateObjects;
    // store path objects if machine is multi hosts
    std::vector<std::string> hostsToCreateObjects;
    if (isMultiHostMachine())
    {
        boost::split(hostsToCreateObjects,
                     OBMC_HOST_INSTANCES, boost::is_any_of(" "));
    }

    // try to detect the image type and the binary image file
    HostImageType imageType(imageDirPath);
    std::string msg = "firmware image file is '" + imageType.imageFile() + '\'';
    log<level::INFO>(msg.c_str());

    auto  imageTypeId = imageType.curTypeString();
    std::vector<std::string> typesToCreate;
    if (imageTypeId.empty() == false)
    {   // only one image type
        typesToCreate.push_back(imageTypeId);
        msg = "Firmware image type: '" + imageType.curTypeString()+ '\'';
        log<level::INFO>(msg.c_str());
    }
    else
    {
        // create objects for all image types and give responsability to user
        log<level::WARNING>("Firmware image type NOT detected");
        typesToCreate  = HostImageType::availableTypes();
    }
    for (const auto& type : typesToCreate)
    {
        imageTypesToCreateObjects.push_back(firmwarePath + '/' + type);
    }
    // create a container object to store all necessary information
    auto hostImageData =
            std::make_unique<FirmwareImageUpdateData>(imageType.curTypeString(),
                                                      imageType.imageFile());
    for (const auto& imgType : imageTypesToCreateObjects)
    {
        if (hostsToCreateObjects.empty() == true)
        {
            createSingleFirmwareObject(imgType, hostImageData.get());
        }
        else
        {
            for (const auto& host : hostsToCreateObjects)
            {
                createSingleFirmwareObject(imgType + '/' + host,
                                           hostImageData.get());
            }
        }
    }
    // store the information for an image
    msg = "creating hosts information for the image id " + versionId;
    log<level::INFO>(msg.c_str());
    this->hostFirmwareObjects.insert(std::make_pair(versionId,
                                                    std::move(hostImageData)));
    /* create a new thread to watch for a possible
     *  image removal before updating all hosts  in case of multi-host machine.
     * Note: when all hosts are updated the image is removed forcing
     *  the same flow to be followed and the thread be terminated.
     */
    auto lambda = [this, versionId](){this->watchHostImageRemoval(versionId);};
    std::thread watchRemovalThread(lambda);
    watchRemovalThread.detach();
}


void ItemUpdater::watchHostImageRemoval(const std::string& versionId)
{
     Activation* hostActivation = activations.find(versionId)->second.get();
     if (hostActivation != nullptr)
     {
         hostActivation->watchHostImageRemoval();
     }
     else
     {
         std::string msg("Could not find Activation object for version id ");
         msg += versionId;
         log<level::ERR>(msg.c_str());
     }
}

void ItemUpdater::createSingleFirmwareObject(const std::string &pathObject,
                                             FirmwareImageUpdateData *container)
{
    auto object = std::make_unique<FirmwareUpdate>(bus, pathObject);
    container->pathObjects.push_back(std::move(object));
    std::string msg = "creating object path: " + pathObject;
    log<level::INFO>(msg.c_str());
}

void ItemUpdater::clearHostFirwareObjects(const std::string &versionId)
{
   hostFirmwareObjects.erase(versionId);
}

FirmwareImageUpdateData *
ItemUpdater::canPerformUpdateFirmware(const std::string& versionId)
{
    std::string  msg;
    auto hostImageData = hostFirmwareObjects.find(versionId)->second.get();
    if (hostImageData == nullptr)
    {
        msg = "firmware update information not found for image id "
                          +  versionId;
        log<level::EMERG>(msg.c_str());
        return nullptr;
    }
    if (hostImageData->image_binay_file.empty() == true)
    {
        msg = "binary image file unkown for the image id "
                               +  versionId;
        log<level::EMERG>(msg.c_str());
        return nullptr;
    }
    // special case: single host with image type does not
    //      require 'Update' property being true
    if (isMultiHostMachine() == false
            && hostImageData->image_type.empty() == false
            && hostImageData->pathObjects.size() == 1)
    {
        hostImageData->pathObjects.at(0)->setUpdateRequired();
    }
    auto counter = hostImageData->pathObjects.size();
    while (counter-- > 0)
    {
        // make sure the image type is correct, when it is unknown the user is
        //  expected to set that using set-propety over the right object path
        if (hostImageData->pathObjects.at(counter)->isUpdateRequired())
        {
            // get the image from object path
            auto str = hostImageData->pathObjects.at(counter)->hostObjectPath();
            auto slash_position = str.find_last_of('/');
            if (isMultiHostMachine() == true)
            {
                str.erase(slash_position, str.size() - slash_position);
                slash_position = str.find_last_of('/');
            }
            auto image_type = str.substr(slash_position+1,
                                         str.size() - slash_position -1);
            if (hostImageData->image_type.empty() == true)
            {
                hostImageData->image_type = image_type;
            }
            else
            {
                if (image_type != hostImageData->image_type)
                {
                    msg = "image id " + versionId +
                          " has different 'image types with the"
                          " property 'Update' set to true";
                    log<level::ERR>(msg.c_str());
                    return nullptr;
                }
            }
            hostImageData->hostsToUpdate++;
        }
    }
    if (hostImageData->hostsToUpdate == 0)
    {
        msg = "image id " + versionId +
                + " needs at least one host with the property"
                  " 'Update' set to true";
        log<level::ERR>(msg.c_str());
        return nullptr;
    }
    if (hostImageData->image_type.empty() == true)
    {
        msg = "could not determine the image type for the image id "
                + versionId;
        log<level::ERR>(msg.c_str());
        return nullptr;
    }
    // OK to perform the host firmware update
    return hostImageData;
}

bool ItemUpdater::isMultiHostMachine() const
{
    bool multihost = false;  // single host is the default
#if defined(OBMC_HOST_INSTANCES)
    if (::strcmp("0", OBMC_HOST_INSTANCES) != 0
            && ::strchr(OBMC_HOST_INSTANCES, ' '))
    {
        multihost = true;
    }
#endif
    return multihost;
}
#endif // HOST_FIRMWARE_UPGRADE

} // namespace updater
} // namespace software
} // namespace phosphor
