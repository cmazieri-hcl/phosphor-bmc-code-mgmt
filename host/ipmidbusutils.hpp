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

#pragma once

#include "qemuanswer.hpp"
#include "ipmiutils.hpp"
#include <memory>


namespace ipmi
{
namespace util
{


class Dbus
{
 public:
    explicit Dbus(sdbusplus::bus::bus& bus);
    virtual ~Dbus();

    /**
     * @brief objectTreeValues() brings a map of objets with their properties
     *                           and values
     * @param service        the service name
     * @param path           the root object path (its children are expected)
     * @param interface [optional] the interface name of a just a substring,
     *                       if empty, properties from all interfaces will be
     *                       considered, otherwise only properties from
     *                       interfaces that match with 'interface'
     * @return a map with 'path' and its chiildren all filled with
     *                    properties and values that match interfaces
     *                    'interface'
     */
    ObjectPropertyStringValue objectTreeValues(const std::string& service,
                                               const std::string& path,
                                               const std::string& interface={});

    /** @brief objectTreeData() builds a simple search engine using the
     *         return from @sa objectTreeValues(), this search engine can  be
     *         used to search for properties x objecs x values,
     *         @sa ObjectDataSearchable
     *  @return the simple search engine object ObjectDataSearchable
     */
    ObjectDataSearchable objectTreeData(const std::string& service,
                                        const std::string& path,
                                        const std::string& interface = {});

    /**
     * @brief getProperties() gets all properties and values regarding inputs
     * @param service    the service name
     * @param path       the object path
     * @param interface  the interface name
     * @return a map of properties and values (which are based on std::variant)
     */
    PropertyMap getProperties(const std::string& service,
                                      const std::string& path,
                                      const std::string& interface);
    /**
     * @brief setQemuAnswerFile() defines a file which contains Qemu answers
     *        If running on emulator does not bring relevant data, it is
     *         possible to have pre defined returns (answers) for emulator
     *         @sa QemuAnswer class for more information
     * @param filename full path name of the file in qemu image
     */
    void setQemuAnswerFile(const std::string& filename);
 protected:
    /**
     * @brief objectTreeValues() brings a map of objets with their properties
     *                           and values
     * @param service        the service name
     * @param path           the root object path (its children are expected)
     * @param interfaceMatch the interface name of a just a substring,
     *                       if empty, properties from all interfaces will be
     *                       considered, otherwise only properties from
     *                       interfaces that match with 'interfaceMatch'
     * @param tree [out] a map with 'path' and its chiildren all filled with
     *                    properties and values that match interfaces
     *                    'interfaceMatch'
     */
    void objectTreeValues(const std::string& service,
                          const std::string& path,
                          const std::string& interfaceMatch,
                          ObjectPropertyStringValue* tree);


    void qemuAnswerForObjectTreeData(const QemuAnswer::AnswerData *answer,
                                     ObjectPropertyStringValue *dataTree);

 private:
    sdbusplus::bus::bus&          _bus;
    std::unique_ptr<QemuAnswer>   _qemu_emulator_returns;
};


} // namespace util
} // namespace ipmi
