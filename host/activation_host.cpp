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
    if ((value != softwareServer::Activation::Activations::Active) &&
            (value != softwareServer::Activation::Activations::Activating))
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
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
    return value;
}


void ActivationHost::flashWrite()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                              SYSTEMD_INTERFACE, "StartUnit");
    auto serviceFileName = this->baseServiceFileName() + ".service";
    method.append(serviceFileName, "replace");
    std::string msg = "launching host flash " + serviceFileName;
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
    std::string str = "Host Software upgrade finished for " + path;
    // Set Activation value to active
    activation(ActivationStateValue::Active);
    log<level::INFO>(str.c_str());

    parent.onActivationDone(versionId);

    /**
            * firmwareVersion used to be biosVersion, check master branch
            * parent.firmwareVersion->version(
            *         parent.versions.find(versionId)->second->version());
            */

/**
    log<level::INFO>("performing cleaning...");
    // just remove the image, the watcher will catch it
    Helper helper(bus);
    helper.removeVersion(versionId);

    **/
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
    // if does NOT start with obmc-flash-host@<ImgId>-<ImgType>[-HostId]
    if (newStateUnit.rfind(baseServiceFile, 0) != 0)
    {
        return;
    }

    activationProgress->progress(100);
    // unsubscribe to systemd signals
    unsubscribeFromSystemdSignals();

    if (newStateResult == "done")
    {
        onFlashWriteSuccess();
    }
    else if (newStateResult == "failed")
    {
        // Set Activation value to Failed
        activation(ActivationStateValue::Failed);
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
    std::string basename = "obmc-flash-host@" + versionId + '-';
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
