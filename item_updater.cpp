#include "config.h"

#include "item_updater.hpp"

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
namespace fs = std::filesystem;

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
            error("Failed to find the needed BMC images.");
            return ItemUpdater::ActivationStatus::invalid;
        }
    }

    return ItemUpdater::ActivationStatus::ready;
}


void ItemUpdater::reset()
{
    // Empty
}


void ItemUpdater::savePriority(const std::string& versionId, uint8_t value)
{
    storePriority(versionId, value);
    helper.setEntry(versionId, value);
}

std::string
ItemUpdater::freePriority(ActivationMap& activationMap,
                               uint8_t value, const std::string& versionId)
{
    std::map<std::string, uint8_t> priorityMap;

    // Insert the requested version and priority, it may not exist yet.
    priorityMap.insert(std::make_pair(versionId, value));

    for (const auto& intf : activationMap)
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
            auto it = activationMap.find(element.first);
            it->second->redundancyPriority.get()->sdbusPriority(
                freePriorityValue);
        }
    }

    std::string lowestVersion = prioritySet.begin()->first;
    if (value == prioritySet.begin()->second)
    {
        lowestVersion = versionId;
    }

    return lowestVersion;
}


void ItemUpdater::removeReadOnlyPartition(std::string versionId)
{
    helper.removeVersion(versionId);
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


void ItemUpdater::createVersion(const SoftwareVersionMessage& versionInfo)
{
    auto versionPtr = std::make_unique<VersionClass>(
                bus, versionInfo.path, versionInfo.version, versionInfo.purpose,
                versionInfo.extendedVersion, versionInfo.filePath,
                std::bind(&ItemUpdater::erase, this, std::placeholders::_1));

    versionPtr->deleteObject =
            std::make_unique<phosphor::software::manager::Delete>
                   (bus, versionInfo.path, *versionPtr);
    versions.insert(std::make_pair(versionInfo.versionId,
                                   std::move(versionPtr)));
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
        erase(deletableIt);
    }

    helper.cleanup();
}


} // namespace updater
} // namespace software
} // namespace phosphor
