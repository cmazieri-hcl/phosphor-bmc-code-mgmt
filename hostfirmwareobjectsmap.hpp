/*
/ Copyright (c) 2019-2021 Facebook Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/


#pragma once

#include "firmwareupdate.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace phosphor
{
namespace software
{
namespace updater
{

/**
 * Keeps the information necessary to handle and flash a single host image
 */
struct FirmwareImageUpdateData
{
    FirmwareImageUpdateData(const std::string& img_type,
                            const std::string& bin_file)
      : image_type(img_type)
      , image_binay_file(bin_file)
      , hostsToUpdate(0)
      , hostsAlreadyUpated(0)
    {
        // empty
    }
    FirmwareImageUpdateData() = delete;
    FirmwareImageUpdateData(const FirmwareImageUpdateData&) = delete;
    FirmwareImageUpdateData& operator=(const FirmwareImageUpdateData&) = delete;
    /**
     * @brief nextHostToUpdateFirmware()
     * @return next host information which
     *        \sa isUpdateRequiredButNotStartedYet() == true
     */
    FirmwareUpdate* nextHostToUpdateFirmware() const
    {
        auto counter = pathObjects.size();
        while (counter-- > 0)
        {
            auto object = pathObjects.at(counter).get();
            if (object->isUpdateRequiredButNotStartedYet() == true)
            {
                return object;
            }
        }
        return nullptr;
    }


    FirmwareUpdate* getOnGoingHostByService(const std::string& serviceName)
    {
        auto myServiceName = serviceName;
        auto counter = pathObjects.size();
        if (counter > 0)
        {
            auto position = myServiceName.find_last_of('.');
            if (position != std::string::npos)
            {
                myServiceName.erase(position);
            }
            // name is obmc-flash-host-cpld@1a56bff3-1.service
            // or obmc-flash-host-software@1a56bff3-bios-1.service
            position = myServiceName.find_last_of('@');
            if (position != std::string::npos)
            {
                position =  myServiceName.find_first_of('-', position+1);
                if (position !=  std::string::npos)
                {
                    auto parameter = myServiceName.substr(position+1);
                    for (position = 0; position < parameter.size(); ++position)
                    {
                        if (parameter[position] == '-')
                        {
                            parameter[position] = '/';
                        }
                    }
                    // use parameter to find the object path
                    auto parameterSize = parameter.size();
                    while (counter-- > 0)
                    {
                        FirmwareUpdate* object = pathObjects.at(counter).get();
                        if (object->isUpdateOnGoing())
                        {
                            const std::string host = object->hostObjectPath();
                            auto hostTail = host.size() - parameterSize;
                            if (hostTail > 0)
                            {
                                position = host.find(parameter, hostTail);
                                if (position == hostTail) // ends with
                                {
                                    return object;
                                }
                            }
                        }
                    }
                }
            }
        }
        return nullptr;
    }


    void setCurrentHostUpdateOnGoing(FirmwareUpdate* host)
    {
        host->setUpdateOnGoing();
    }

    void setUpdateCompleted(FirmwareUpdate* host)
    {
        host->setUpdateCompleted();
        hostsAlreadyUpated++;
    }

    /**
     * @brief areAllHostsUpdated()
     * @return true if all hosts from the current image type have been updated
     */
    bool areAllHostsUpdated()
    {
        // this function does not know if the host object path contains the host
        // id or not, in other words,
        //    it does not know if the machine is single os multi host
        auto counter = pathObjects.size();
        decltype(counter) image_type_pos = 0;
        decltype(counter) image_type_len      = 0;
        bool allHostSameImageTypeUpdated = false;
        // first step, set position and length of the image
        // from in the host  object path
        if (counter > 0)
        {
            allHostSameImageTypeUpdated = true; // there are hosts, set true
            auto host           = pathObjects.at(0)->hostObjectPath();
            auto slash_position = host.find_last_of('/');
            image_type_pos = slash_position+1;
            image_type_len      = host.size() - slash_position -1;
            auto imgType        = host.substr(image_type_pos, image_type_len);
            // if they do not match, it is a multi-host machine
            if (imgType != this->image_type)
            {
                 host.erase(slash_position, host.size() - slash_position);
                 slash_position      = host.find_last_of('/');
                 image_type_pos = slash_position+1;
                 image_type_len      = host.size() - slash_position -1;
            }
        }
        while (counter-- > 0)
        {
            auto const & host = pathObjects.at(counter)->hostObjectPath();
            auto imgType = host.substr(image_type_pos, image_type_len);
            if (imgType == this->image_type
                    && pathObjects.at(counter)->isUpdated() == false)
            {
                // image type matches but the host is not updated yet
                allHostSameImageTypeUpdated  = false;
                break;
            }
        }
        return allHostSameImageTypeUpdated;
    }

    /**
     * @brief isUpdateActivationDone()
     *        Checks if an Activation step is completed.
     *        It does not mean all hosts have been updated
     *           @sa areAllHostsUpdated
     * @return true if a Activation action is complete
     */
    bool isUpdateActivationDone() const
    {
        return hostsAlreadyUpated > 0
                 && hostsAlreadyUpated == hostsToUpdate;
    }

    int stepHostUpdate() const
    {
        int percent = 0;
        if (hostsToUpdate > 0 && hostsAlreadyUpated > 0)
        {
            percent = (hostsAlreadyUpated * 100) / hostsToUpdate;
        }
        return percent;
    }

    std::string baseServiceFileName(const std::string& imageId)
    {
        std::string basename = "obmc-flash-host-" + image_type + "@" + imageId;
        return basename;
    }

    std::string                                  image_type;
    const std::string                            image_binay_file;
    int                                          hostsToUpdate;
    int                                          hostsAlreadyUpated;
    std::vector<std::unique_ptr<FirmwareUpdate>> pathObjects;
};

/**
 * The map string is the main host object path which has the 'Activation'
 * A map is necessary since more than a image can be copied into /tmp/images
 *   at same time.
 */
using HostFirmwareObjectsMap =
        std::map<const std::string, std::unique_ptr<FirmwareImageUpdateData>>;

} // namespace updater
} // namespace software
} // namespace phosphor
