#include "activation_host.hpp"

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
    return value;
}

void ActivationHost::flashWrite()
{

}

void ActivationHost::onFlashWriteSuccess()
{

}

void ActivationHost::onStateChanges(sdbusplus::message::message &)
{

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


} // namespace updater
} // namespace software
} // namespace phosphor
