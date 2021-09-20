#include "item_updater_helper.hpp"

#include "utils.hpp"
#include <filesystem>

namespace phosphor
{
namespace software
{
namespace updater
{

void Helper::setEntry(const std::string& /* entryId */, uint8_t /* value */)
{
    // Empty
}

void Helper::clearEntry(const std::string& /* entryId */)
{
    // Empty
}

void Helper::cleanup()
{
    // Empty
}

void Helper::factoryReset()
{
    // Set openbmconce=factory-reset env in U-Boot.
    // The init will cleanup rwfs during boot.
    utils::execute("/sbin/fw_setenv", "openbmconce", "factory-reset");
}

/**
 * @brief Helper::removeVersion() removes the /tmp/images/versionId
 * @param versionId
 */
void Helper::removeVersion(const std::string&  versionId)
{
    std::filesystem::path imageDirPath = std::string{IMG_UPLOAD_DIR};
    imageDirPath /= versionId;
    if (std::filesystem::exists(imageDirPath))
    {
        std::filesystem::remove_all(imageDirPath);
    }
}

void Helper::updateUbootVersionId(const std::string& /* versionId */)
{
    // Empty
}

void Helper::mirrorAlt()
{
    // Empty
}

} // namespace updater
} // namespace software
} // namespace phosphor
