#include "item_updater_host.hpp"

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
}

void ItemUpdaterHost::freeSpace(Activation &caller)
{
    (void) caller;
}

void ItemUpdaterHost::freePriority(uint8_t value, const std::string &versionId)
{
    (void)value;
    (void) versionId;
}

void ItemUpdaterHost::reset()
{
    // Empty
}

void ItemUpdaterHost::createActivation(sdbusplus::message::message &msg)
{
    (void) msg;
}



} // namespace updater
} // namespace software
} // namespace phosphor

