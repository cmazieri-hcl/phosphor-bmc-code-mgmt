/*
/ Copyright (c) 2019-2021 Facebook Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/


#include "ipmidbusutils.hpp"
#include <sdbusplus/bus.hpp>

namespace phosphor
{
namespace software
{
namespace updater
{


/**
 * @brief The ImagetypeHostsAssociation class
 *
 * Responsabilities:
 * 1. Identify the 'image type' of the image in /tmp/images
 * 2. Create the association between hosts and the current 'image type'.
 *
 * Flow:
 * 1. Read FRU devices from Entity Manager with IMAGETYPE Decorator
 * 2. Create a list of the image types detected from (1).
 * 3. Create association between hosts and the current 'image type',
 *    the host object path always contains the 'image type', multi-host machines
 *    will also have '_<hostId>'. Examples:
 *         single-host machines will have path such as:
 *             /xyz/openbmc_project/software/1a56bff3/cpld
 *
 *         multi-host machines will have paths such as:
 *             /xyz/openbmc_project/software/1a56bff3/cpld_1
 */
class ImagetypeHostsAssociation
{
  public:
       using ImageTypeList     = std::map<std::string, int>;
       using HostImageTypeList = std::map<std::string, ImageTypeList>;
       using EntityManagerDict = ipmi::util::ObjectDataSearchable;

       ImagetypeHostsAssociation(sdbusplus::bus::bus& bus,
                                 const std::string& imgPath);

       /**
        * @brief associatedHostObjectsPath()
        *
        * @return a list of associated Host Objects Path
        */
       std::vector<std::string>  associatedHostObjectsPath() const;

       /**
        * @brief imageType()
        * @return the 'image type' detected or an empty string if not detected
        */
       std::string               imageType() const;

       /**
        * @brief isValid
        * @return true if there is 'image type' and at least a host associated
        */
       bool                      isValid()  const;

  protected:
       ImagetypeHostsAssociation(sdbusplus::bus::bus& bus);
       void setData(const std::string& imgPath);
       EntityManagerDict getActiveHostsFromEntityManager();
       std::string       readImageTypeFromManifestFile();
       ImageTypeList     createImageTypeList(const EntityManagerDict &);
       bool              identifyImageType(const ImageTypeList&);
       void              readTargethostsFromManifestFile(EntityManagerDict *);
       void              removeHostsImageTypeNotIn(EntityManagerDict *dit);
       void              createHostsImageTypeAssociation(EntityManagerDict *,
                                                        std::string image_type);
  private:
       sdbusplus::bus::bus &     _bus;
       std::string               _imagePath;
       std::string               _imageType;
       std::vector<std::string>  _host_obj_paths;
       HostImageTypeList         _image_type_list_per_host;
};


} // namespace updater
} // namespace software
} // namespace phosphor

