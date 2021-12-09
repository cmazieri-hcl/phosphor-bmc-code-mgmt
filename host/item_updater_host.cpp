#include "config.h"


#include "item_updater_host.hpp"
#include "imagetype_host_association.hpp"
#include "activation_host.hpp"

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
          || imgMsg.isPurposeHOST() == false
          || multiActivations.find(imgMsg.versionId) != multiActivations.end())
    {
        return;
    }

    ImagetypeHostsAssociation hostsAssociation(bus, imgMsg.filePath);
    auto  hosts_assoc_imgType =  hostsAssociation.associatedHostsIds();

    // finally creates Activation for hosts in the list
     if (hostsAssociation.isValid() && hosts_assoc_imgType.empty() == false)
     {
         ItemUpdater::createVersion(imgMsg);
         auto activationState = ActivationStateValue::Ready;
         AssociationList associations = {};
         ActivationMap   activations;
         for (const auto& imgType_host : hosts_assoc_imgType)
         {
             auto host_obj_path  =  imgMsg.path + '/' + imgType_host;
             activations.insert(
                  std::make_pair(host_obj_path,
                                 std::make_unique<ActivationHost>
                                 (bus, host_obj_path, *this, imgMsg.versionId,
                                 activationState, associations)));
         }
         this->multiActivations[imgMsg.versionId] = std::move(activations);
     }
}


void ItemUpdaterHost::onActivationDone(const std::string &imageVersionId)
{
    bool allDone = true;
    for (const auto &activationMap : this->multiActivations)
    {
        if (activationMap.first == imageVersionId)
        {
            for (const auto& activation : activationMap.second)
            {
                if (activation.second->activation()
                        != ActivationStateValue::Active)
                {
                    allDone = false;
                    break;
                }
            }
        }
    }
    if (allDone)
    {
        this->multiActivations.erase(imageVersionId);
    }
}



} // namespace updater
} // namespace software
} // namespace phosphor

