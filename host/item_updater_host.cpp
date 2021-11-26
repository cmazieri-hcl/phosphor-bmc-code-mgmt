#include "config.h"


#include "item_updater_host.hpp"
#include "imagetype_host_association.hpp"

#include <phosphor-logging/lg2.hpp>


namespace phosphor
{
namespace software
{
namespace updater
{


ItemUpdaterHost::ItemUpdaterHost(sdbusplus::bus::bus &bus,
                                 const std::string &path)
    : ItemUpdater(bus, path)
{

}

void ItemUpdaterHost::erase(std::string entryId)
{
    (void)entryId;
    //TODO: implement it here
}


void ItemUpdaterHost::freeSpace(Activation &caller)
{
    (void)caller;
    //Empty
}


void ItemUpdaterHost::freePriority(uint8_t value, const std::string &versionId)
{
    for (auto& activations : multiActivations)
    {
        ItemUpdater::freePriority(activations.second, value, versionId);
    }
}


void ItemUpdaterHost::createActivation(sdbusplus::message::message &msg)
{
    SoftwareVersionMessage imgMsg(msg);
    if (imgMsg.isValid() == false
          || multiActivations.find(imgMsg.versionId) != multiActivations.end())
    {
        return;
    }

    ImagetypeHostsAssociation hostsAssociation(bus, imgMsg.path);
    auto  hosts_assoc_imgType =  hostsAssociation.associatedHostsIds();

    // finally creates Activation for hosts in the list
    if (hostsAssociation.isValid() && hosts_assoc_imgType.empty() == false)
    {
        ItemUpdater::createVersion(imgMsg);
        auto activationState = server::Activation::Activations::Ready;
        AssociationList associations = {};
        for (const auto& host : hosts_assoc_imgType)
        {
            (void) host;
            (void) activationState;
            (void) associations;
        }
    }
}



} // namespace updater
} // namespace software
} // namespace phosphor

