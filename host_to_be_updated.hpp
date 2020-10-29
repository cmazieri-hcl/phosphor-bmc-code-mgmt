#pragma once

#include "xyz/openbmc_project/Software/HostToBeUpdated/server.hpp"

#include <sdbusplus/bus.hpp>

#include <functional>
#include <string>

namespace phosphor
{
namespace software
{
namespace manager
{

using HostToBeUpdatedInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Software::server::HostToBeUpdated>;

class HostToBeUpdated : public HostToBeUpdatedInherit
{
  public:
    /** @brief Constructs HostToBeUpdated Software Manager
     *
     * @param[in] bus            - The D-Bus bus object
     * @param[in] objPath        - The D-Bus object path
     * @param[in] toBeUpdated    - The bool value
     */
    HostToBeUpdated(sdbusplus::bus::bus& bus, const std::string& objPath,
                    const bool toBeUpdated) :
        HostToBeUpdatedInherit(bus, (objPath).c_str(), true),
        isUpdated(toBeUpdated)
    {
        // Set properties.
        hostToBeUpdated(toBeUpdated);
        // Emit deferred signal.
        emit_object_added();
    }

  private:
    /** @brief This Version's version string */
    const bool isUpdated;
};

} // namespace manager
} // namespace software
} // namespace phosphor
