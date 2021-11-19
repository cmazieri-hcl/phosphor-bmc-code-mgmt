#include "config.h"
#include "item_updater.hpp"

#include <phosphor-logging/lg2.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{


SoftwareVersionMessage::SoftwareVersionMessage(sdbusplus::message::message& msg)
{
    readMessage(msg);
}


bool SoftwareVersionMessage::isPurposeBMC() const
{
    return purpose == VersionPurpose::BMC;
}


bool SoftwareVersionMessage::isPurposeHOST() const
{
    return purpose == VersionPurpose::Host;
}


bool SoftwareVersionMessage::isPurposeSYSTEM() const
{
     return purpose == VersionPurpose::System;
}


bool SoftwareVersionMessage::isPurposeUnknown() const
{
     return purpose == VersionPurpose::Unknown;
}


bool SoftwareVersionMessage::isValid() const
{
    return purpose != VersionPurpose::Unknown
            && version.empty()   == false
            && path.empty()      == false
            && filePath.empty()  == false
            && versionId.empty() == false;
}


void  SoftwareVersionMessage::readMessage(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    std::map<std::string, std::map<std::string, std::variant<std::string>>>
        interfaces;
    msg.read(objPath, interfaces);
    path = std::move(objPath);
    for (const auto& intf : interfaces)
    {
        if (intf.first == VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Purpose")
                {
                    auto value =
                          server::Version::convertVersionPurposeFromString(
                                        std::get<std::string>(property.second));
                    if (value == VersionPurpose::BMC  ||
                        value == VersionPurpose::Host ||
                        value == VersionPurpose::System)
                    {
                        purpose = value;
                    }
                }
                else if (property.first == "Version")
                {
                    version = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == FILEPATH_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "Path")
                {
                    filePath = std::get<std::string>(property.second);
                }
            }
        }
        else if (intf.first == EXTENDED_VERSION_IFACE)
        {
            for (const auto& property : intf.second)
            {
                if (property.first == "ExtendedVersion")
                {
                    extendedVersion = std::get<std::string>(property.second);
                }
            }
        }
    } // end for

    if (purpose != VersionPurpose::Unknown)
    {
        auto pos = path.rfind("/");
        if (pos == std::string::npos)
        {
            lg2::error("No version id found in object path: {PATH}",
                       "PATH", path);
            return;
        }
        versionId = path.substr(pos + 1);
    }
}


} // namespace updater
} // namespace software
} // namespace phosphor
