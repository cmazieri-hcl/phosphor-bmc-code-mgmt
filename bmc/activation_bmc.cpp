#include "config.h"

#include "activation_bmc.hpp"
#include "images.hpp"
#include "serialize.hpp"
#include "item_updater_bmc.hpp"
#include "msl_verify.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Software/Version/error.hpp>


namespace
{
constexpr auto PATH_INITRAMFS = "/run/initramfs";
} // namespace


namespace phosphor
{
namespace software
{
namespace updater
{

using namespace phosphor::logging;

ActivationBmc::ActivationBmc(sdbusplus::bus::bus &bus,
                             const std::string &path,
                             ItemUpdater &parent,
                             std::string &versionId,
                             ActivationStateValue activationStatus,
                             AssociationList &assocs)
    : Activation(bus, path, parent, versionId, activationStatus, assocs)
{
    // Empty
}


ActivationStateValue
ActivationBmc::activation(ActivationStateValue value)
{
    if ((value != ActivationStateValue::Active) &&
        (value != ActivationStateValue::Activating))
    {
        redundancyPriority.reset(nullptr);
    }

    if (value == ActivationStateValue::Activating)
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
                    ActivationStateValue::Failed);
            }
        }
#endif
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
                ActivationStateValue::Failed);
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
            ActivationStateValue::Active);
#endif
    }
    else
    {
        activationBlocksTransition.reset(nullptr);
        activationProgress.reset(nullptr);
    }
    return softwareServer::Activation::activation(value);
}


void ActivationBmc::flashWrite()
{
    namespace fs = std::filesystem;
    using namespace phosphor::software::image;

    // For static layout code update, just put images in /run/initramfs.
    // It expects user to trigger a reboot and an updater script will program
    // the image to flash during reboot.
    fs::path uploadDir(IMG_UPLOAD_DIR);
    fs::path toPath(PATH_INITRAMFS);

    for (const auto& bmcImage : parent.imageUpdateList)
    {
        fs::copy_file(uploadDir / versionId / bmcImage, toPath / bmcImage,
                      fs::copy_options::overwrite_existing);
    }
}


void ActivationBmc::onFlashWriteSuccess()
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
    parent.deleteImageManagerObject();

    // Create active association
    parent.createActiveAssociation(path);

    // Create updateable association as this
    // can be re-programmed.
    parent.createUpdateableAssociation(path);

    if (Activation::checkApplyTimeImmediate() == true)
    {
        lg2::info("Image Active and ApplyTime is immediate; rebooting BMC.");
        rebootBmc();
    }
    else
    {
        lg2::info("BMC image ready; need reboot to get activated.");
    }

    activation(ActivationStateValue::Active);
}


void ActivationBmc::onStateChanges(sdbusplus::message::message &)
{
    // Empty
}


void ActivationBmc::unitStateChange(sdbusplus::message::message &)
{
    // Empty
}


void ActivationBmc::rebootBmc()
{
    auto method = bus.new_method_call(SYSTEMD_BUSNAME, SYSTEMD_PATH,
                                      SYSTEMD_INTERFACE, "StartUnit");
    method.append("force-reboot.service", "replace");
    try
    {
        auto reply = bus.call(method);
    }
    catch (const sdbusplus::exception::exception& e)
    {
        lg2::alert("Error in trying to reboot the BMC. The BMC needs to be manually "
              "rebooted to complete the image activation. {ERROR}",
              "ERROR", e);
        report<InternalFailure>();
    }
}


} // namespace updater
} // namespace software
} // namespace phosphor
