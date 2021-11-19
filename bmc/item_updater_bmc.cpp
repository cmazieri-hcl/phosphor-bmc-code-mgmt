#include "config.h"

#include "item_updater_bmc.hpp"
#include "activation_bmc.hpp"

#include "images.hpp"
#include "serialize.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Software/ExtendedVersion/server.hpp"
#include "xyz/openbmc_project/Software/Version/server.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Image/error.hpp>

#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <string>


namespace phosphor
{
namespace software
{
namespace updater
{

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Software::Image::Error;
using namespace phosphor::software::image;
using NotAllowed = sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;

ItemUpdaterBmc::ItemUpdaterBmc(sdbusplus::bus::bus &bus,
                               const std::string &path)
    : ItemUpdater(bus, path)
{
    setBMCInventoryPath();
    processBMCImage();
    restoreFieldModeStatus();
}


void ItemUpdaterBmc::reset()
{
    helper.factoryReset();

    info("BMC factory reset will take effect upon reboot.");
}


void ItemUpdaterBmc::setBMCInventoryPath()
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
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error in mapper GetSubTreePath: {ERROR}", "ERROR", e);
        return;
    }

    return;
}


void ItemUpdaterBmc::mirrorUbootToAlt()
{
    helper.mirrorAlt();
}


void ItemUpdaterBmc::updateUbootEnvVars(const std::string& versionId)
{
    helper.updateUbootVersionId(versionId);
}


void ItemUpdaterBmc::resetUbootEnvVars()
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


void ItemUpdaterBmc::processBMCImage()
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
        error("Failed to prepare dir: {ERROR}", "ERROR", e);
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
            iter.path().native().compare(0, BMC_RO_PREFIX_LEN, BMC_ROFS_PREFIX))
        {
            // Get the version to calculate the id
            fs::path releaseFile(OS_RELEASE_FILE);
            auto osRelease = iter.path() / releaseFile.relative_path();
            if (!fs::is_regular_file(osRelease))
            {
                error("Failed to read osRelease: {PATH}", "PATH", osRelease);

                // Try to get the version id from the mount directory name and
                // call to delete it as this version may be corrupted. Dynamic
                // volumes created by the UBI layout for example have the id in
                // the mount directory name. The worst that can happen is that
                // erase() is called with an non-existent id and returns.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                erase(id);

                continue;
            }
            auto version = VersionClass::getBMCVersion(osRelease);
            if (version.empty())
            {
                error("Failed to read version from osRelease: {PATH}", "PATH",
                      osRelease);

                // Try to delete the version, same as above if the
                // OS_RELEASE_FILE does not exist.
                auto id = iter.path().native().substr(BMC_RO_PREFIX_LEN);
                erase(id);

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
                    ACTIVATION_FWD_ASSOCIATION, ACTIVATION_REV_ASSOCIATION,
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
                std::bind(&ItemUpdater::erase, this, std::placeholders::_1));
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
                        bus, path, *this, id, activationState, associations)));

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
                        error(
                            "Unable to restore priority from file for {VERSIONID}",
                            "VERSIONID", id);
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
            this->processBMCImage();
        }
        catch (const std::exception& e)
        {
            error("Exception during processing: {ERROR}", "ERROR", e);
        }
    }

    mirrorUbootToAlt();
    return;
}

bool ItemUpdaterBmc::fieldModeEnabled(bool value)
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


void ItemUpdaterBmc::restoreFieldModeStatus()
{
    std::ifstream input("/dev/mtd/u-boot-env");
    std::string envVar;
    std::getline(input, envVar);

    if (envVar.find("fieldmode=true") != std::string::npos)
    {
        ItemUpdater::fieldModeEnabled(true);
    }
}

void ItemUpdaterBmc::freePriority(uint8_t value, const std::string &versionId)
{
    auto lowestVersion = ItemUpdater::freePriority(activations, value,
                                                   versionId);
    updateUbootEnvVars(lowestVersion);
}


void ItemUpdaterBmc::createActivation(sdbusplus::message::message& msg)
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
                    if (value == VersionPurpose::BMC ||
#ifdef HOST_BIOS_UPGRADE
                        value == VersionPurpose::Host ||
#endif
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
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
        error("No version id found in object path: {PATH}", "PATH", path);
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
                                ACTIVATION_REV_ASSOCIATION, bmcInventoryPath));
        }

        activations.insert(std::make_pair(
            versionId,
            std::make_unique<ActivationBmc>(bus, path, *this, versionId,
                                         activationState, associations)));

        auto versionPtr = std::make_unique<VersionClass>(
            bus, path, version, purpose, extendedVersion, filePath,
            std::bind(&ItemUpdater::erase, this, std::placeholders::_1));
        versionPtr->deleteObject =
            std::make_unique<phosphor::software::manager::Delete>(bus, path,
                                                                  *versionPtr);
        versions.insert(std::make_pair(versionId, std::move(versionPtr)));
    }
    return;
}



void ItemUpdaterBmc::freeSpace(Activation& caller)
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


void ItemUpdaterBmc::erase(std::string entryId)
{
    // Find entry in versions map
    auto it = versions.find(entryId);
    if (it != versions.end())
    {
        if (it->second->isFunctional() && ACTIVE_BMC_MAX_ALLOWED > 1)
        {
            error(
                "Version ({VERSIONID}) is currently running on the BMC; unable to remove.",
                "VERSIONID", entryId);
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
        error(
            "Failed to find version ({VERSIONID}) in item updater activations map; unable to remove.",
            "VERSIONID", entryId);
    }
    else
    {
        removeAssociations(iteratorActivations->second->path);
        iteratorActivations->second->deleteImageManagerObject();
        this->activations.erase(entryId);
    }
    this->resetUbootEnvVars();

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

        error(
            "Failed to find version ({VERSIONID}) in item updater versions map; unable to remove.",
            "VERSIONID", entryId);
    }

    helper.clearEntry(entryId);

    return;
}


} // namespace updater
} // namespace software
} // namespace phosphor
