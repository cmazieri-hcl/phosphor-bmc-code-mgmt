
#include "activation.hpp"

#include "images.hpp"
#include "item_updater.hpp"
#include "msl_verify.hpp"
#include "serialize.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include "image_verify.hpp"
#endif

extern boost::asio::io_context& getIOContext();

namespace phosphor
{
namespace software
{
namespace updater
{

PHOSPHOR_LOG2_USING;
using namespace phosphor::logging;


#ifdef WANT_SIGNATURE_VERIFY
namespace control = sdbusplus::xyz::openbmc_project::Control::server;
#endif

void Activation::subscribeToSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Subscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        if (e.name() != nullptr &&
            strcmp("org.freedesktop.systemd1.AlreadySubscribed", e.name()) == 0)
        {
            // If an Activation attempt fails, the Unsubscribe method is not
            // called. This may lead to an AlreadySubscribed error if the
            // Activation is re-attempted.
        }
        else
        {
            error("Error subscribing to systemd: {ERROR}", "ERROR", e);
        }
    }

    return;
}

void Activation::unsubscribeFromSystemdSignals()
{
    auto method = this->bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                            SYSTEMD_INTERFACE, "Unsubscribe");
    try
    {
        this->bus.call_noreply(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error unsubscribing from systemd signals: {ERROR}", "ERROR", e);
    }

    return;
}


void Activation::deleteImageManagerObject()
{
    // Call the Delete object for <versionID> inside image_manager
    auto method = this->bus.new_method_call(VERSION_BUSNAME, path.c_str(),
                                            "xyz.openbmc_project.Object.Delete",
                                            "Delete");
    try
    {
        bus.call_noreply(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error deleting image ({PATH}) from image manager: {ERROR}",
              "PATH", path, "ERROR", e);
        return;
    }
}

/**
 * @brief It does nothing and MUST be reimplemented in children classes
 * @param value
 * @return
 */
ActivationStateValue
Activation::activation(ActivationStateValue value)
{
     return softwareServer::Activation::activation(value);
}

auto Activation::requestedActivation(RequestedActivations value)
    -> RequestedActivations
{
    rwVolumeCreated = false;
    roVolumeCreated = false;
    ubootEnvVarsUpdated = false;

    if ((value == softwareServer::Activation::RequestedActivations::Active) &&
        (softwareServer::Activation::requestedActivation() !=
         softwareServer::Activation::RequestedActivations::Active))
    {
        if ((softwareServer::Activation::activation() ==
             ActivationStateValue::Ready) ||
            (softwareServer::Activation::activation() ==
             ActivationStateValue::Failed))
        {
            Activation::activation(ActivationStateValue::Activating);
        }
    }
    return softwareServer::Activation::requestedActivation(value);
}

uint8_t RedundancyPriority::priority(uint8_t value)
{
    // Set the priority value so that the freePriority() function can order
    // the versions by priority.
    auto newPriority = softwareServer::RedundancyPriority::priority(value);
    parent.parent.savePriority(parent.versionId, value);
    parent.parent.freePriority(value, parent.versionId);
    return newPriority;
}

uint8_t RedundancyPriority::sdbusPriority(uint8_t value)
{
    parent.parent.savePriority(parent.versionId, value);
    return softwareServer::RedundancyPriority::priority(value);
}


#ifdef WANT_SIGNATURE_VERIFY
bool Activation::verifySignature(const fs::path& imageDir,
                                 const fs::path& confDir)
{
    using Signature = phosphor::software::image::Signature;

    Signature signature(imageDir, confDir);

    return signature.verify();
}

void Activation::onVerifyFailed()
{
    error("Error occurred during image validation");
    report<InternalFailure>();
}
#endif

void ActivationBlocksTransition::enableRebootGuard()
{
    info("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);
}

void ActivationBlocksTransition::disableRebootGuard()
{
    info("BMC activation has ended - BMC reboots are re-enabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-disable.service", "replace");
    bus.call_noreply(method);
}

bool Activation::checkApplyTimeImmediate()
{
    auto service = utils::getService(bus, applyTimeObjPath, applyTimeIntf);
    if (service.empty())
    {
        info("Error getting the service name for BMC image ApplyTime. "
             "The BMC needs to be manually rebooted to complete the image "
             "activation if needed immediately.");
    }
    else
    {

        auto method = bus.new_method_call(service.c_str(), applyTimeObjPath,
                                          dbusPropIntf, "Get");
        method.append(applyTimeIntf, applyTimeProp);

        try
        {
            auto reply = bus.call(method);

            std::variant<std::string> result;
            reply.read(result);
            auto applyTime = std::get<std::string>(result);
            if (applyTime == applyTimeImmediate)
            {
                return true;
            }
        }
        catch (const sdbusplus::exception::exception& e)
        {
            error("Error in getting ApplyTime: {ERROR}", "ERROR", e);
        }
    }
    return false;
}


} // namespace updater
} // namespace software
} // namespace phosphor
