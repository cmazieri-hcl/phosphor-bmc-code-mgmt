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

     /**
      * @brief Reboot the BMC. Called when ApplyTime is immediate.
      *
      * @return none
      **/
     void rebootBmc();

     /** @brief Overloaded Activation property setter function
      *
      * @param[in] value - One of Activation::Activations
      *
      * @return Success or exception thrown
      */
     ActivationStateValue activation(ActivationStateValue value) override;

     /** @brief Overloaded write flash function */
     void flashWrite() override;

     /**
      * @brief Handle the success of the flashWrite() function
      *
      * @details Perform anything that is necessary to mark the activation
      * successful after the image has been written to flash. Sets the Activation
      * value to Active.
      */
     void onFlashWriteSuccess() override;

     /** @brief Overloaded function that acts on service file state changes */
     void onStateChanges(sdbusplus::message::message&) override;

     /** @brief Check if systemd state change is relevant to this object
      *
      * Instance specific interface to handle the detected systemd state
      * change
      *
      * @param[in]  msg       - Data associated with subscribed signal
      *
      */
     void unitStateChange(sdbusplus::message::message& msg) override;
};


} // namespace updater
} // namespace software
} // namespace phosphor
