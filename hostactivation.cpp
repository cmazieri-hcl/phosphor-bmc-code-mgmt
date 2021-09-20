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

#include "config.h"

#ifdef HOST_FIRMWARE_UPGRADE

#include "activation.hpp"
#include "images.hpp"
#include "item_updater.hpp"
#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <filesystem>
#include <unistd.h>

extern boost::asio::io_context& getIOContext();

namespace phosphor
{
namespace software
{
namespace updater
{
namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;
namespace fs = std::filesystem;
using namespace phosphor::software::image;
using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;


void Activation::flashWriteHost()
{
    decltype(m_hostFirmwareData->nextHostToUpdateFirmware()) hostToUpdate =
                m_hostFirmwareData->nextHostToUpdateFirmware();
    int counter = 0;
    while (hostToUpdate != nullptr )
    {
        auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                          SYSTEMD_INTERFACE, "StartUnit");
        auto firmwareServiceFile =
                m_hostFirmwareData->baseServiceFileName(versionId);
        if (parent.isMultiHostMachine())
        {
            auto str = hostToUpdate->hostObjectPath();
            auto slash_position = str.find_last_of('/');
            auto host = str.substr(slash_position+1,
                                   str.size() - slash_position -1);
            firmwareServiceFile += "-" + host;
            if (counter++ > 0 )
            {
                ::sleep(1); // give some to the previous be launched
            }
        }
        firmwareServiceFile += ".service";
        method.append(firmwareServiceFile, "replace");
        std::string msg = "launching host flash " + firmwareServiceFile;
        log<level::INFO>(msg.c_str());
        // save information this host object is being upated
        m_hostFirmwareData->setCurrentHostUpdateOnGoing(hostToUpdate);
        try
        {
            bus.call(method);
        }
        catch (const SdBusError& e)
        {
            log<level::ERR>(e.description());
            report<InternalFailure>();
        }
        hostToUpdate = m_hostFirmwareData->nextHostToUpdateFirmware();
    }
}


void Activation::onHostStateChanges(sdbusplus::message::message& msg)
{
    uint32_t newStateID{};
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);
    auto baseServiceFile = m_hostFirmwareData->baseServiceFileName(versionId);
    // checks if newStateUnit starts with obmc-flash-host<img-type>@<versionId>
    if (newStateUnit.rfind(baseServiceFile, 0) != 0)
    {
        return;
    }
    if (newStateResult == "done")
    {
        auto currentHostBeingUpdated =
                m_hostFirmwareData->getOnGoingHostByService(newStateUnit);
        m_hostFirmwareData->setUpdateCompleted(currentHostBeingUpdated);
        decltype(activationProgress->progress()) currentProgres =
                activationProgress->progress();
        auto oneHostPercent = m_hostFirmwareData->stepHostUpdate();
        currentProgres = currentProgres <  oneHostPercent ?
                    oneHostPercent : currentProgres + oneHostPercent;
        activationProgress->progress(currentProgres);
        std::string str = "Firmware upgrade finished for "
                + currentHostBeingUpdated->hostObjectPath();
        log<level::INFO>(str.c_str());
        str = "Total firmware upgrade status is %"
                + std::to_string(currentProgres);
        log<level::INFO>(str.c_str());
        if (m_hostFirmwareData->isUpdateActivationDone() == true)
        {
            // unsubscribe to systemd signals
            unsubscribeFromSystemdSignals();
            // Set Activation value to active
            activation(softwareServer::Activation::Activations::Active);
            log<level::INFO>("host(s) software upgrade completed.");
            /**
             * firmwareVersion used to be biosVersion, check master branch
             * parent.firmwareVersion->version(
             *         parent.versions.find(versionId)->second->version());
             */
            if (m_hostFirmwareData->areAllHostsUpdated() == true)
            {
                log<level::INFO>("performing cleaning...");
                // Remove version object from image manager
                // it also removes the image from disk
                deleteImageManagerObject();
                parent.erase(this->versionId);
                parent.clearHostFirwareObjects(versionId);
                m_hostFirmwareData = nullptr;
                // Delete the uploaded activation
                auto lbdaErase = [this](){this->parent.erase(this->versionId);};
                boost::asio::io_service io_service;
                io_service.post(lbdaErase);
            }
        }
    }
    else if (newStateResult == "failed")
    {
        // Set Activation value to Failed
        activation(softwareServer::Activation::Activations::Failed);
        log<level::ERR>("Firmware upgrade failed.");
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor

#endif // HOST_FIRMWARE_UPGRADE

