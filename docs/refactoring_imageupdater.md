# Refactoring plan: generic ItemUpdater for Hosts software update

Git repo:  https://github.com/openbmc/phosphor-bmc-code-mgmt.git

A new *ItemUpdater* instance (another process) will be created to manage hosts
software update. It requires refactoring on current code that basically
regards to using polymorphism for both *ItemUpdater* and *Activation*
classes.

## Main information

1. `DBUS address`:  xyz.openbmc_project.Software.HOST.Updater
2. `Binary file`:  /usr/bin/phosphor-host-image-updater, the BMC instance
                   may or or not be renamed to phosphor-bmc-image-updater.
3. `Service file`:  xyz.openbmc_project.Software.HOST.Updater.service
4. `Main source`:   host/item_updater_host_main.cpp


## ItemUpdater class refactoring

The *ItemUpdater* class becomes an abstract class and new classes
*ItemUpdaterBmc* and *ItemUpdaterHost* will be created.


### File organization regarding ItemUpdater class
| Action | File | Class | New File|
-------|-----------|------|------|
| renamed |item_updater_main.cpp|ItemUpdaterBmc|bmc/item_updater_bmc_main.cpp|
| new     ||ItemUpdaterHost|host/item_updater_host_main.cpp|
| new    ||ItemUpdaterBmc|bmc/item_updater_bmc.hpp|
| new    ||ItemUpdaterBmc|bmc/item_updater_bmc.cpp|
| new    ||ItemUpdaterHost|host/item_updater_host.hpp|
| new    ||ItemUpdaterHost|host/item_updater_host.cpp|


### Methods and variables moved from ItemUpdater class to ItemUpdaterBmc class

| Item | Type |
|------|------|
|void processBMCImage()| method |
|bool isLowestPriority(uint8_t value)| method|
|void updateUbootEnvVars(const std::string& versionId)| method|
|void resetUbootEnvVars()| method|
|void reset() override| method|
|bool fieldModeEnabled(bool value) override| method|
|setBMCInventoryPath()| method|
|void restoreFieldModeStatus()| method|
|void mirrorUbootToAlt()| method|
|std::string bmcInventoryPath| variable|
|ActivationMap activations| variable |

### Methods from ItemUpdater class turned as pure virtual

| Method | Scope|
|------|------|
|void freePriority(uint8_t value, const std::string& versionId)|public|
|void erase(std::string entryId)|public|
|void freeSpace(Activation& caller)| public|
|void createActivation(sdbusplus::message::message& msg)| private|


### ItemUpdaterBmc class

This class aims to behave exeatcly as the old *ItemUpdater* class.



### ItemUpdaterHost class

This class will manage multiple Activations per a single image id.


## Activation class refactoring

The *Activation* class becomes abstract and new classes *ActivationBmc* and
*ActivationHost* will be created.

### Methods removed from *Activation* class

1. The method deleteImageManagerObject() can be moved into *ItemUpdater* class.
2. The method rebootBmc() can be moved to *ActivationBmc* class.


### Methods moved from *Activation* class to children classes
| Method | action|
|------|------|
|void onFlashWriteSuccess()|pure virtual and moved to Flash interface/class|
|void onStateChanges()|pure virtual from Flash  interface/class |
|void flashWrite()|pure virtual from Flash  interface/class |
|Activations activation(Activations value) override|virtual|
|void unitStateChange(sdbusplus::message::message &|pure virtual|


### File organization regarding Activation class
| Action | Class | New File|
-------|-----------|------|
| new    |ActivationBmc|bmc/activation_bmc.hpp|
| new    |ActivationBmc|bmc/activation_bmc.cpp|
| new    |ActivationHost|host/activation_host.hpp|
| new    |ActivationHost|host/activation_host.cpp|
