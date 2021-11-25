#include "config.h"


#include "imagetype_host_association.hpp"
#include "hostimagetype.hpp"

#include <phosphor-logging/lg2.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <fstream>

namespace phosphor
{
namespace software
{
namespace updater
{

// for test purposes
ImagetypeHostsAssociation::ImagetypeHostsAssociation(sdbusplus::bus::bus &bus)
    : _bus(bus)
{
    // Empty
}

void ImagetypeHostsAssociation::setData(const std::string &imgPath)
{
    _imagePath = imgPath;
}


ImagetypeHostsAssociation::ImagetypeHostsAssociation(sdbusplus::bus::bus &bus,
                                                     const std::string &imgPath)
    : _bus(bus)
    , _imagePath(imgPath)
{
    { // block to create and destroy temporary variables
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
        if (identifyImageType(image_type_list) == false)
        {
            lg2::error("Could not identify the type of the image");
            return;
        }
        if (image_type_list.find(this->_imageType) == image_type_list.end())
        {
            lg2::error("Image type detected does is not allowed");
            return;
        }

        // remove hosts that do NOT accept the detected ImageType
        removeHostsImageTypeNotIn(&entiManagerDict);
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
                                      "/xyz/openbmc_project/inventory/system",
                                      "Inventory.Decorator.Compatible");

    // searching all FRU devices which has IMAGETYPE items in
    // xyz.openbmc_project.Inventory.Decorator.Compatible interface
    // see as example <bmc>/entity-manager/configurations/FBYV2.json
    auto objList = dict.searchObjectContains("Names", "IMAGETYPE=");

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
            auto notImageTypeIterator = it;
            for (const auto& host : objList)
            {
                if (host == it->first)
                {
                    notImageTypeIterator = dict.objectData.rend();
                    break;
                }
            }
            if (notImageTypeIterator != dict.objectData.rend())
            {
                dict.objectData.erase(notImageTypeIterator->first);
            }
            else
            {
                ++it;
            }
        }
    }

    return dict;
}


std::string ImagetypeHostsAssociation::readImageTypeFromManifestFile()
{
    std::string manifest_imgType;

    std::string manifest(_imagePath);
    manifest += "/MANIFEST";
    std::ifstream stream(manifest, std::ios_base::in);
    if (stream.is_open() == true)
    {
        char line[512];
        const char * ImageType_flag = "ImageType=";
        const int    ImageType_flag_size = ::strlen(ImageType_flag);
        while (stream.good())
        {
            stream.getline(line, sizeof(line));
            if (::strncmp(line, ImageType_flag, ImageType_flag_size) == 0)
            {
                char image_type_value[24] = {0};
                ::sscanf(&line[ImageType_flag_size], "%s", image_type_value);
                manifest_imgType = image_type_value;
                boost::to_lower(manifest_imgType);
                break;
            }
        }
        stream.close();
    }
    return manifest_imgType;
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
        auto image_type_string = dict.getValue(host.first, "Names");
        if (image_type_string.empty() == false)
        {
            std::vector<std::string> image_type_list;
            boost::algorithm::split(image_type_list, image_type_string,
                                    boost::is_any_of(" "));
            for (const auto& single_image_type : image_type_list)
            {
                auto last_dot = single_image_type.rfind('.');
                if (last_dot != std::string::npos)
                {
                    auto separator = single_image_type.find('-', last_dot);
                    if (separator == std::string::npos)
                    {
                        separator = single_image_type.find('_', last_dot);
                    }
                    auto start_image_string = separator != std::string::npos ?
                                separator + 1 :
                                last_dot  + 1;
                    auto imgType = single_image_type.substr(start_image_string);
                    boost::to_lower(imgType);

                    // set lists
                    single_host_list[imgType] = 1;
                    global_imagetype_list[imgType] = 1;
                }
            }
            // this is the list will be used for association
            this->_image_type_list_per_host[host.first] = single_host_list;
        }
    }
    return global_imagetype_list;
}


bool ImagetypeHostsAssociation::identifyImageType(const ImageTypeList & list)
{
    this->_imageType = readImageTypeFromManifestFile();
    if (this->_imageType.empty() == true)
    {
        std::vector<std::string> array;
        for (const auto& img_type : list)
        {
            array.push_back(img_type.first);
        }
        HostImageType image_identifier(_imagePath, array);
        this->_imageType =  image_identifier.curTypeString();
    }
    return this->_imageType.empty() == false;
}


void
ImagetypeHostsAssociation::removeHostsImageTypeNotIn(EntityManagerDict *dict)
{
    for (auto it = _image_type_list_per_host.rbegin();
           it != _image_type_list_per_host.rend(); )
    {
        // if that host does not support the current image type, remove it
        // to not allow flashing on it
        if (it->second.find(_imageType) == it->second.end())
        {
            if (dict != nullptr)
            {
                dict->objectData.erase(it->first);
            }
            _image_type_list_per_host.erase(it->first);
        }
        else
        {
            ++it;
        }
    }
}



} // namespace updater
} // namespace software
} // namespace phosphor

