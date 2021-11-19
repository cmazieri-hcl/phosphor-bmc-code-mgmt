#include "config.h"

#include "item_updater_host.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/manager.hpp>

boost::asio::io_context& getIOContext()
{
    static boost::asio::io_context io;
    return io;
}

int main()
{
    sdbusplus::asio::connection bus(getIOContext());

    // Add sdbusplus ObjectManager.
    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);

    phosphor::software::updater::ItemUpdaterHost updater(bus, SOFTWARE_OBJPATH);

    bus.request_name(HOST_BUS_UPDATER);

    getIOContext().run();

    return 0;
}
