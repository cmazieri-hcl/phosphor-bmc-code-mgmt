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

#include "xyz/openbmc_project/Software/FirmwareUpdate/server.hpp"

#include <sdbusplus/bus.hpp>

#include <functional>
#include <string>

namespace phosphor
{
namespace software
{
namespace updater
{

using FirmwareUpdateInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::FirmwareUpdate>;

class FirmwareUpdate : public FirmwareUpdateInherit
{
  public:   
    /** @brief Constructs FirmwareUpdate Software Manager
     *
     * @param[in] bus            - The D-Bus bus object
     * @param[in] objPath        - The D-Bus object path
     * @param[in] toBeUpdated    - The bool value
     */
    FirmwareUpdate(sdbusplus::bus::bus& bus,
                   const std::string& objPath);

    /**
     * @brief Sets properties saying the update is required
     */
    void setUpdateRequired();

    /**
     * @brief  Sets properties saying the update is performed
     */
    void setUpdateCompleted();

    /**
     * @brief Sets properties saying the update is ongoing
     */
    void setUpdateOnGoing();

    /**
     * @brief isUpdateRequired
     * @return true when 'Update' property is set to true
     */
    bool isUpdateRequired() const;

    /**
     * @brief isUpdateRequiredButNotStartedYet()
     * @return true then firmware update must be performed
     */
    bool isUpdateRequiredButNotStartedYet() const;

    /**
     * @brief isFirmwareUpdated
     * @return true when state is Done saying the image update has been performed
     */
    bool isFirmwareUpdated() const;

    /**
     * @brief hostObjectPath()
     * @return the object having at least the image type
     *         multi host machines will have the host id as the last element
     *
     */
    const std::string hostObjectPath() const;

private:
    /** @brief store host object path */
    const std::string  _hostObjPath;
};

} // namespace updater
} // namespace software
} // namespace phosphor
