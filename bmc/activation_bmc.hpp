#pragma once

#include "activation.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{



class ActivationBmc : public Activation
{
 public:
     ActivationBmc(sdbusplus::bus::bus& bus,
                   const std::string& path,
                   ItemUpdater& parent,
                   std::string& versionId,
                   ActivationStateValue activationStatus,
                   AssociationList& assocs);
};


} // namespace updater
} // namespace software
} // namespace phosphor
