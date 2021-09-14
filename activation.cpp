#include "activation.hpp"

#include "images.hpp"
#include "item_updater.hpp"
#include "msl_verify.hpp"
#include "serialize.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/exception.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>

#ifdef WANT_SIGNATURE_VERIFY
#include "image_verify.hpp"
#endif

#include <unistd.h>

namespace phosphor
{
namespace software
{
namespace updater
{

namespace softwareServer = sdbusplus::xyz::openbmc_project::Software::server;

using namespace phosphor::logging;
using sdbusplus::exception::SdBusError;
using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;

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
    catch (const SdBusError& e)
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
            log<level::ERR>("Error subscribing to systemd",
                            entry("ERROR=%s", e.what()));
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
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in unsubscribing from systemd signals",
                        entry("ERROR=%s", e.what()));
    }

    return;
}

auto Activation::activation(Activations value) -> Activations
{
    if ((value != softwareServer::Activation::Activations::Active) &&
        (value != softwareServer::Activation::Activations::Activating))
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == softwareServer::Activation::Activations::Activating)
    {
#ifdef WANT_SIGNATURE_VERIFY
        fs::path uploadDir(IMG_UPLOAD_DIR);
        if (!verifySignature(uploadDir / versionId, SIGNED_IMAGE_CONF_PATH))
        {
            onVerifyFailed();
            // Stop the activation process, if fieldMode is enabled.
            if (parent.control::FieldMode::fieldModeEnabled())
            {
                return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
            }
        }
#endif

#ifdef HOST_FIRMWARE_UPGRADE
        auto purpose = parent.versions.find(versionId)->second->purpose();
        if (purpose == VersionPurpose::Host)
        {
            if (activationProgress == nullptr)
            {
                activationProgress =
                    std::make_unique<ActivationProgress>(bus, path);
            }

           m_hostFirmwareData = parent.canPerformUpdateFirmware(path);
           if (m_hostFirmwareData != nullptr)
           {
               std::string msg = "preparing to update "
                             + m_hostFirmwareData->image_type
                             + " firmware on "
                             + std::to_string(m_hostFirmwareData->hostsToUpdate)
                             + " host(s)";
               log<level::INFO>(msg.c_str());
               // Enable systemd signals
               subscribeToSystemdSignals();

               // Set initial progress
               activationProgress->progress(2);

               // Initiate image writing to flash
               flashWriteHost();
               return softwareServer::Activation::activation(value);
           }
           return softwareServer::Activation::activation(
                    softwareServer::Activation::Activations::Failed);
        }
#endif // HOST_FIRMWARE_UPGRADE

        auto versionStr = parent.versions.find(versionId)->second->version();

        if (!minimum_ship_level::verify(versionStr))
        {
            using namespace phosphor::logging;
            using IncompatibleErr = sdbusplus::xyz::openbmc_project::Software::
                Version::Error::Incompatible;
            using Incompatible =
                xyz::openbmc_project::Software::Version::Incompatible;

            report<IncompatibleErr>(
                prev_entry<Incompatible::MIN_VERSION>(),
                prev_entry<Incompatible::ACTUAL_VERSION>(),
                prev_entry<Incompatible::VERSION_PURPOSE>());
            return softwareServer::Activation::activation(
                softwareServer::Activation::Activations::Failed);
        }

        if (!activationProgress)
        {
            activationProgress =
                std::make_unique<ActivationProgress>(bus, path);
        }

        if (!activationBlocksTransition)
        {
            activationBlocksTransition =
                std::make_unique<ActivationBlocksTransition>(bus, path);
        }

        activationProgress->progress(10);

        parent.freeSpace(*this);

        // Enable systemd signals
        Activation::subscribeToSystemdSignals();

        flashWrite();

#if defined UBIFS_LAYOUT || defined MMC_LAYOUT

        return softwareServer::Activation::activation(value);

#else // STATIC_LAYOUT

        onFlashWriteSuccess();
        return softwareServer::Activation::activation(
            softwareServer::Activation::Activations::Active);
#endif
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}

void Activation::onFlashWriteSuccess()
{
    activationProgress->progress(100);

    activationBlocksTransition.reset(nullptr);
    activationProgress.reset(nullptr);

    rwVolumeCreated = false;
    roVolumeCreated = false;
    ubootEnvVarsUpdated = false;
    Activation::unsubscribeFromSystemdSignals();

    storePurpose(versionId, parent.versions.find(versionId)->second->purpose());

    if (!redundancyPriority)
    {
        redundancyPriority =
            std::make_unique<RedundancyPriority>(bus, path, *this, 0);
    }

    // Remove version object from image manager
    Activation::deleteImageManagerObject();

    // Create active association
    parent.createActiveAssociation(path);

    // Create updateable association as this
    // can be re-programmed.
    parent.createUpdateableAssociation(path);

    if (Activation::checkApplyTimeImmediate() == true)
    {
        log<level::INFO>("Image Active. ApplyTime is immediate, "
                         "rebooting BMC.");
        Activation::rebootBmc();
    }
    else
    {
        log<level::INFO>("BMC image ready, need reboot to get activated.");
    }

    activation(softwareServer::Activation::Activations::Active);
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
    catch (const SdBusError& e)
    {
        log<level::ERR>("Error in Deleting image from image manager",
                        entry("VERSIONPATH=%s", path.c_str()));
        return;
    }
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
             softwareServer::Activation::Activations::Ready) ||
            (softwareServer::Activation::activation() ==
             softwareServer::Activation::Activations::Failed))
        {
            Activation::activation(
                softwareServer::Activation::Activations::Activating);
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

void Activation::unitStateChange(sdbusplus::message::message& msg)
{
    if (softwareServer::Activation::activation() !=
        softwareServer::Activation::Activations::Activating)
    {
        return;
    }

#ifdef HOST_FIRMWARE_UPGRADE
    auto purpose = parent.versions.find(versionId)->second->purpose();
    if (purpose == VersionPurpose::Host)
    {
        onHostStateChanges(msg);
        return;
    }
#endif // HOST_FIRMWARE_UPGRADE

    onStateChanges(msg);

    return;
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
    log<level::ERR>("Error occurred during image validation");
    report<InternalFailure>();
}
#endif

void ActivationBlocksTransition::enableRebootGuard()
{
    log<level::INFO>("BMC image activating - BMC reboots are disabled.");

    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("reboot-guard-enable.service", "replace");
    bus.call_noreply(method);
}

void ActivationBlocksTransition::disableRebootGuard()
{
    log<level::INFO>("BMC activation has ended - BMC reboots are re-enabled.");

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
        log<level::INFO>("Error getting the service name for BMC image "
                         "ApplyTime. The BMC needs to be manually rebooted to "
                         "complete the image activation if needed "
                         "immediately.");
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
        catch (const SdBusError& e)
        {
            log<level::ERR>("Error in getting ApplyTime",
                            entry("ERROR=%s", e.what()));
        }
    }
    return false;
}

#ifdef HOST_FIRMWARE_UPGRADE
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
    if (newStateUnit.rfind(baseServiceFile, 0) == 0)
    {

        if (newStateResult == "done")
        {
            auto currentHostBeingUpdated =
                    m_hostFirmwareData->getHostByService(newStateUnit);
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
                log<level::INFO>("Firmware upgrade completed successfully.");
                /**
                 * hostFirmwareObjects used to be biosVersion, check master
                 *  branch
                parent.hostFirmwareObjects->version(
                            parent.versions.find(versionId)->second->version());
                */
                if (m_hostFirmwareData->areAllHostsUpdated() == true)
                {
                    // Remove version object from image manager
                    deleteImageManagerObject();
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
    return;
}
#endif // HOST_FIRMWARE_UPGRADE

void Activation::rebootBmc()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("force-reboot.service", "replace");
    try
    {
        auto reply = bus.call(method);
    }
    catch (const SdBusError& e)
    {
        log<level::ALERT>("Error in trying to reboot the BMC. "
                          "The BMC needs to be manually rebooted to complete "
                          "the image activation.");
        report<InternalFailure>();
    }
}

} // namespace updater
} // namespace software
} // namespace phosphor
