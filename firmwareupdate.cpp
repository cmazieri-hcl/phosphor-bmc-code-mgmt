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


#include "firmwareUpdate.hpp"

const std::string NoneState{"None"};
const std::string OnGoingState{"OnGoing"};
const std::string DoneState{"Done"};

namespace phosphor
{
namespace software
{
namespace updater
{

FirmwareUpdate::FirmwareUpdate(sdbusplus::bus::bus& bus,
                               const std::string& objPath)
    :  FirmwareUpdateInherit(bus, (objPath).c_str(), true)
{
    setUpdateRequired();
    emit_object_added();   // Emit deferred signal
}


void FirmwareUpdate::setUpdateRequired()
{
    FirmwareUpdateInherit::update(true, true);  // skip signal
    FirmwareUpdateInherit::state(NoneState, true);
}

void FirmwareUpdate::setUpdateCompleted()
{
    FirmwareUpdateInherit::update(false, true);
    FirmwareUpdateInherit::state(DoneState, true);
}

void FirmwareUpdate::setUpdateOnGoing()
{
   FirmwareUpdateInherit::state(OnGoingState, true);
}

bool FirmwareUpdate::isFirmwareUpdated() const
{
    return FirmwareUpdateInherit::state() == DoneState;
}

} // namespace updater
} // namespace software
} // namespace phosphor
