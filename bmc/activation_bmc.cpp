#include "activation_bmc.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{


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


} // namespace updater
} // namespace software
} // namespace phosphor
