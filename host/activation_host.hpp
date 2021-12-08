#pragma once

#include "activation.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{



class ActivationHost : public Activation
{
 public:
     ActivationHost(sdbusplus::bus::bus& bus,
                    const std::string& path,
                    ItemUpdater& parent,
                    std::string& versionId,
                    ActivationStateValue activationStatus,
                    AssociationList& assocs);

     ActivationStateValue activation(ActivationStateValue value) override;

     void flashWrite() override;

     void onFlashWriteSuccess() override;

     void onStateChanges(sdbusplus::message::message&) override;

     void unitStateChange(sdbusplus::message::message& msg) override;
};


} // namespace updater
} // namespace software
} // namespace phosphor
