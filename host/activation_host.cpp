#include "config.h"

#include "activation_host.hpp"
#include "item_updater_host.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <sdbusplus/bus.hpp>

using namespace phosphor::logging;

namespace phosphor
{
namespace software
{
namespace updater
{


ActivationHost::ActivationHost(sdbusplus::bus::bus &bus,
                               const std::string &path,
                               ItemUpdater &parent,
                               std::string &versionId,
                               ActivationStateValue activationStatus,
                               AssociationList &assocs)
    : Activation(bus, path, parent, versionId, activationStatus, assocs)
{
    // Empty
}


ActivationStateValue ActivationHost::activation(ActivationStateValue value)
{
    if ((value != ActivationStateValue::Active) &&
            (value != ActivationStateValue::Activating))
    {
        redundancyPriority.reset(nullptr);
    }

    auto curActivationStateValue = softwareServer::Activation::activation();
    if (value == ActivationStateValue::Activating
            && curActivationStateValue != ActivationStateValue::Activating)
    {
        if (activationProgress == nullptr)
        {
            activationProgress =
                    std::make_unique<ActivationProgress>(bus, path);
        }
        // Enable systemd signals
        subscribeToSystemdSignals();
        // Set initial progress
        activationProgress->progress(2);
        // Initiate image writing to flash
        flashWrite();
    }
    return softwareServer::Activation::activation(value);
}


void ActivationHost::flashWrite()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                              SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFileName = this->baseServiceFileName() + ".service";
    method.append(serviceFileName, "replace");
    std::string msg = "launching Host Software upgrade " + serviceFileName;
    log<level::INFO>(msg.c_str());
    try
    {
        bus.call(method);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(e.description());
        report<InternalFailure>();
    }
}


void ActivationHost::onFlashWriteSuccess()
{
    // Set Activation value to active
    softwareServer::Activation::activation(ActivationStateValue::Active);
    std::string str = "Host Software upgrade finished for " + path;
    log<level::INFO>(str.c_str());

    parent.onActivationDone(versionId);
}


void ActivationHost::onStateChanges(sdbusplus::message::message &msg)
{
    sdbusplus::message::object_path newStateObjPath;
    std::string newStateUnit{};
    std::string newStateResult{};

    // Read the msg and populate each variable
    uint32_t newStateID{};
    msg.read(newStateID, newStateObjPath, newStateUnit, newStateResult);
    auto baseServiceFile = this->baseServiceFileName();
    // does NOT start with obmc-flash-host-software@<ImgId>-<ImgType>[-HostId]
    if (newStateUnit.rfind(baseServiceFile, 0) != 0)
    {
        return;
    }

    //  set completed progress
    activationProgress->progress(100);
    // unsubscribe to systemd signals
    unsubscribeFromSystemdSignals();
    activationProgress.reset(nullptr);

    if (newStateResult == "done")
    {
        onFlashWriteSuccess();
    }
    else if (newStateResult == "failed")
    {
        // Set Activation value to Failed
        softwareServer::Activation::activation(ActivationStateValue::Failed);
        log<level::ERR>("Host Software upgrade failed.");
    }
}


void ActivationHost::unitStateChange(sdbusplus::message::message &msg)
{
    if (softwareServer::Activation::activation() !=
        ActivationStateValue::Activating)
    {
        return;
    }
    onStateChanges(msg);
}


std::string ActivationHost::baseServiceFileName()
{
    std::string basename = "obmc-flash-host-software@" + versionId + '-';
    auto slash_position = path.find_last_of('/');
    auto imageType_host = path.substr(slash_position+1,
                                     path.size() - slash_position -1);
    auto host_separator = imageType_host.find('_');
    if (host_separator != std::string::npos)
    {
        imageType_host.replace(host_separator, 1, "-");
    }
    basename += imageType_host;
    return basename;
}


} // namespace updater
} // namespace software
} // namespace phosphor
