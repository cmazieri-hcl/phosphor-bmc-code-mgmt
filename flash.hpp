#pragma once

#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{

/**
 *  @class Flash
 *  @brief Contains flash management functions.
 *  @details The software class that contains functions to interact
 *           with the flash.
 */
class Flash
{
  public:
    /* Destructor */
    virtual ~Flash() = default;

    /**
     * @brief Writes the image file(s) to flash
     */
    virtual void flashWrite() = 0;

    /**
     * @brief Takes action when the state of the activation service file changes
     */
    virtual void onStateChanges(sdbusplus::message::message& msg) = 0;

    /**
     * @brief Handle the success of the flashWrite() function
     *
     * @details Perform anything that is necessary to mark the activation
     * successful after the image has been written to flash. Sets the Activation
     * value to Active.
     */
    virtual void onFlashWriteSuccess() = 0;
};

} // namespace updater
} // namespace software
} // namespace phosphor
