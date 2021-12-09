#pragma once

#include "item_updater.hpp"

namespace phosphor
{
namespace software
{
namespace updater
{

class ItemUpdaterHost : public ItemUpdater
{
 public:
    ItemUpdaterHost(sdbusplus::bus::bus& bus, const std::string& path);

 public: // reimplementations from parent class
    void erase(std::string entryId) override;
    void freeSpace(Activation& caller) override;
    void freePriority(uint8_t value, const std::string& versionId) override;
    void onActivationDone(const std::string& imageVersionId) override;

 protected: // reimplementations from parent class
    void createActivation(sdbusplus::message::message& msg) override;

 private:
    /** @brief Persistent map of Activation D-Bus objects and their
     * version id */
     MultiActivation multiActivations;

};

} // namespace updater
} // namespace software
} // namespace phosphor

