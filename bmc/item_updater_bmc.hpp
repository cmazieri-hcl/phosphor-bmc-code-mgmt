#pragma once

#include "item_updater.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{


class ItemUpdaterBmc : public ItemUpdater
{
 public:
    ItemUpdaterBmc(sdbusplus::bus::bus& bus, const std::string& path);

    /**
     * @brief Create and populate the active BMC Version.
     */
    void processBMCImage();

    /**
     * @brief Updates the U-Boot variables to point to the requested
     *        versionId, so that the systems boots from this version on
     *        the next reboot.
     *
     * @param[in] versionId - The version to point the system to boot from.
     */
    void updateUbootEnvVars(const std::string& versionId);

    /**
     * @brief Updates the uboot variables to point to BMC version with lowest
     *        priority, so that the system boots from this version on the
     *        next boot.
     */
    void resetUbootEnvVars();

    /** @brief Copies U-Boot from the currently booted BMC chip to the
     *  alternate chip.
     */
    void mirrorUbootToAlt();

    /** @brief Restores field mode status on reboot. */
    void restoreFieldModeStatus();

 private:
    /** @brief Sets the BMC inventory item path under
     *  /xyz/openbmc_project/inventory/system/chassis/. */
    void setBMCInventoryPath();

    bool fieldModeEnabled(bool value);

    /** @brief BMC factory reset - marks the read-write partition for
     * recreation upon reboot. */
    void reset() override;

 public: // reimplementations from parent class
    void erase(std::string entryId) override;
    void freeSpace(Activation& caller) override;
    void freePriority(uint8_t value, const std::string& versionId) override;
    void onActivationDone(const std::string& imageVersionId) override;

 protected: // reimplementations from parent class
    void createActivation(sdbusplus::message::message& msg) override;

 private:
    /** @brief The path to the BMC inventory item. */
    std::string bmcInventoryPath;

    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
    ActivationMap  activations;

};

} // namespace updater
} // namespace software
} // namespace phosphor

