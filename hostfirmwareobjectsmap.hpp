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

#include "firmwareUpdate.hpp"
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
 * Keeps the information necessary to handle and flash a single host firmware image
 */
struct  FirmwareImageUpdateData
{
    FirmwareImageUpdateData(const std::string& img_type, const std::string& bin_file)
    : image_type(img_type), image_binay_file(bin_file)
    {
        // empty
    }  
    const std::string                            image_type;
    const std::string                            image_binay_file;
    std::vector<std::unique_ptr<FirmwareUpdate>> pathObjects;
};

/**
 * The map string is the main host object path which has the 'Activation' property
 *   A map is necessary since more than one image can be copied into /tmp/images at same time
 */
using HostFirmwareObjectsMap = std::map<const std::string, std::unique_ptr<FirmwareImageUpdateData>>;

} // namespace updater
} // namespace software
} // namespace phosphor
