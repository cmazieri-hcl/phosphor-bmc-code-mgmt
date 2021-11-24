#include "config.h"


#include "imagetype_host_association.hpp"

#include <phosphor-logging/lg2.hpp>


namespace phosphor
{
namespace software
{
namespace updater
{

ImagetypeHostsAssociation::ImagetypeHostsAssociation(sdbusplus::bus::bus &bus,
                                                     const std::string &imgPath)
    : _bus(bus)
    , _imagePath(imgPath)
{
    auto entiManagerDict = getActiveHostsFromEntityManager();
    if (entiManagerDict.empty() == true)
    {
        lg2::error("No devices/hosts found on Entity Manager");
        return;
    }

    auto image_type_list = createImageTypeList(entiManagerDict);
    if (image_type_list.empty() == true)
    {
        lg2::error("No image types defined in devices/hosts");
        return;
    }
}


bool ImagetypeHostsAssociation::isValid() const
{
    return   _imageType.empty() == false
            && _host_obj_paths.empty() == false;
}


std::vector<std::string>
ImagetypeHostsAssociation::associatedHostObjectsPath() const
{
    return _host_obj_paths;
}


std::string ImagetypeHostsAssociation::imageType() const
{
    return _imageType;
}


//------------------------------------------------------
/**
 * @brief ImagetypeHostsAssociation::getActiveHostsFromEntityManager
 *
 * @return a dictionary of devices from Entity Manager that has the  interface
 * xyz.openbmc_project.Inventory.Decorator.Compatible with Items containg the
 * IMAGETYPE, example:
 *    IMAGETYPE=com.facebook.Software.Action.ImageType.DeltaLayer-Bios
 */
//------------------------------------------------------
ImagetypeHostsAssociation::EntityManagerDict
ImagetypeHostsAssociation::getActiveHostsFromEntityManager()
{
    ipmi::util::Dbus dbus(_bus);
    // the third parameter which is interface can be substring
    auto dict = dbus.objectTreeData("xyz.openbmc_project.EntityManager",
                                      "/xyz/openbmc_project/EntityManager",
                                      "Inventory.Decorator.Compatible");

    // searching all FRU devices which has IMAGETYPE items in
    // xyz.openbmc_project.Inventory.Decorator.Compatible interface
    // see as example <bmc>/entity-manager/configurations/FBYV2.json
    auto objList = dict.searchObject("Names", "IMAGETYPE=");

    if (objList.empty() == true)
    {
        dict.clear(); // make sure no items in the dictionary
        return dict;
    }

    //remove other items that do not have the IMAGETYPE, it should not happen
    // but is necessary to check
    if (objList.size() < dict.objectData.size())
    {
        for (auto it = dict.objectData.rbegin(); it != dict.objectData.rend();)
        {
            auto notImageTypeIterator = dict.objectData.rend();
            for (const auto& host : objList)
            {
                if (host == it->first)
                {
                    notImageTypeIterator = it;
                    break;
                }
            }
            ++it;
            if (notImageTypeIterator != dict.objectData.rend())
            {
                dict.objectData.erase(notImageTypeIterator->first);
            }
        }
    }

    return dict;
}


//------------------------------------------------------
/**
 * @brief ImagetypeHostsAssociation::createImageTypeList
 * @param dict
 *
 *  Builds the _image_type_list_per_host for each host
 *
 * @return a global ImageTypeList for all hosts found
 */
//------------------------------------------------------
ImagetypeHostsAssociation::ImageTypeList
ImagetypeHostsAssociation::createImageTypeList(const EntityManagerDict &dict)
{
    ImageTypeList  global_imagetype_list;
    for (const auto& host : dict.objectData)
    {
        ImageTypeList single_host_list;
        auto image_type_string = dict.searchObject(host.first, "Names");
        if (image_type_string.empty() == false)
        {

        }
    }
    return global_imagetype_list;
}



} // namespace updater
} // namespace software
} // namespace phosphor

