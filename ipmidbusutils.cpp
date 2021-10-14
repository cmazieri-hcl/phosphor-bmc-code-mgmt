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

#include "config.h"
#include "ipmidbusutils.hpp"
#include <boost/foreach.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>


namespace
{

std::vector<std::string>
readXml(std::istream& is,
                    const std::string& interfaceMatch,
                    std::vector<std::string> *ifs  /*out*/ )
{
    // populate tree structure pt
    using boost::property_tree::ptree;
    ptree pt;
    read_xml(is, pt);
    // traverse pt
    std::vector<std::string> children;
    const ptree & formats =  pt.get_child("node");
    BOOST_FOREACH(ptree::value_type const& f, formats)
    {
        if (f.first == "node")
        {
            std::string child = f.second.get("<xmlattr>.name", "");
            if (child.empty() == false)
            {
                children.push_back(child);
            }
        }
        else if (ifs != nullptr && f.first == "interface")
        {
           std::string interface = f.second.get("<xmlattr>.name", "");
           if (interfaceMatch.empty() == true
                   || interface.find(interfaceMatch) != std::string::npos)
           {
               ifs->push_back(interface);
           }
        }
    }
    return children;
}

} // namespace


namespace ipmi
{
namespace util
{


Dbus::Dbus(sdbusplus::bus::bus &bus)
    : _bus(bus),
      _qemu_emulator_returns(nullptr)
{
    // Empty
}


Dbus::~Dbus()
{
    // Empty
}


ObjectPropertyStringValue
Dbus::objectTreeValues(const std::string &service,
                       const std::string &path,
                       const std::string &interface)
{
    ObjectPropertyStringValue tree;
    auto emulator_answer = qemuAnswer(__FUNCTION__, service, path ,interface);
    if (emulator_answer != nullptr)
    {
        qemuAnswerForObjectTreeData(emulator_answer, &tree);
    }
    else
    {
        objectTreeValues(service, path, interface, &tree);
    }
#if defined(PRINT_DATA_STRUCTURES) // set in development environment only
    printf("\n#function and parameters, the function key\n");
    printf("[objectTreeValues %s %s %s]\n",
           service.c_str(), path.c_str(), interface.c_str());
    ipmi::util::printObjectPropertyStringValue(tree);
#endif
    return tree;
}


ObjectDataSearchable
Dbus::objectTreeData(const std::string &service,
                     const std::string &path,
                     const std::string &interface)
{
    ObjectDataSearchable objectsData;
    auto emulator_answer = qemuAnswer(__FUNCTION__, service, path ,interface);
    if (emulator_answer != nullptr)
    {
        qemuAnswerForObjectTreeData(emulator_answer, &objectsData.objectData);
    }
    else
    {
        objectsData.objectData = objectTreeValues(service,  path, interface);
    }
    return objectsData;
}


PropertyMap
Dbus::getProperties(const std::string &service,
                    const std::string &path,
                    const std::string &interface)
{
    PropertyMap properties;
    auto method = _bus.new_method_call(service.c_str(), path.c_str(),
                                         "org.freedesktop.DBus.Properties",
                                         "GetAll");
    method.append(interface);
    try
    {
        auto reply = _bus.call(method);
        reply.read(properties);

    } catch (...)
    {
        // Empty
    }
    return properties;
}


void Dbus::setQemuAnswerFile(const std::string &filename)
{
    if (QemuAnswer::isEmulatorInstanceQEMU() == true)
    {
        std::unique_ptr<QemuAnswer> other(new QemuAnswer(filename));
        if (other->isEmpty() == false)
        {
            _qemu_emulator_returns = std::move(other);
        }
        // else let's std::unique_ptr destroy itself
    }
}


void Dbus::objectTreeValues(const std::string &service,
                            const std::string &path,
                            const std::string &interfaceMatch,
                            ObjectPropertyStringValue *tree)
{
    auto msg = _bus.new_method_call(service.c_str(), path.c_str(),
                                    "org.freedesktop.DBus.Introspectable",
                                    "Introspect");
    try
    {
        std::string xml;
        auto response = _bus.call(msg);
        response.read(xml);
        std::istringstream xml_stream(xml);
      //  printf("%s\n", xml.c_str());
        std::vector<std::string> interfaceList;
        auto children = readXml(xml_stream, interfaceMatch, &interfaceList);
        for (const auto & intf_name : interfaceList)
        {
            auto propertiesValues = getProperties(service, path, intf_name);
            if (propertiesValues.empty() == false)
            {
                PropertyStringMap stringValues;
                copyPropertyMapToPropertyStringMap(propertiesValues,
                                                   &stringValues);
                (*tree)[path] = stringValues;
            }
        }
        for (auto const& child:  children)
        {
            std::string child_path(path);
            child_path += '/' + child;
            objectTreeValues(service, child_path, interfaceMatch, tree);
        }
    }
    catch (...)
    {
        // Empty
    }
}


void Dbus::qemuAnswerForObjectTreeData(const QemuAnswer::AnswerData *answer,
                                  ObjectPropertyStringValue *dataTree)
{
    std::string  objectPathKey{"__no_key__"};
    PropertyStringMap  propertyValuesMap;
    for (const std::string& line : answer->lines_data)
    {
        auto equal_sign = line.find('=');
        if (equal_sign != std::string::npos)
        {
            auto property = line.substr(0, equal_sign);
            auto value = line.substr(equal_sign+1);
            boost::algorithm::trim(property);
            boost::algorithm::trim(value);
            propertyValuesMap[property] = value;
        }
        else
        {
            if (propertyValuesMap.empty() == false)
            {
                 (*dataTree)[objectPathKey] = propertyValuesMap;
                 // clear to store next object path data;
                 propertyValuesMap.clear();
            }
            objectPathKey = line;
            boost::algorithm::trim(objectPathKey);
        }
    }
    // last object path stored data
    if (propertyValuesMap.empty() == false)
    {
         (*dataTree)[objectPathKey] = propertyValuesMap;
    }
}


const QemuAnswer::AnswerData *
Dbus::qemuAnswer(const std::string &func,
                 const std::string &param1,
                 const std::string &param2,
                 const std::string &param3)
{
    const QemuAnswer::AnswerData *answer = nullptr;
    auto qemu_returns = _qemu_emulator_returns.get();
    if (qemu_returns != nullptr)
    {
        std::vector<std::string> funcKey{func};
        if (param1.empty() == false) {funcKey.push_back(param1);}
        if (param2.empty() == false) {funcKey.push_back(param2);}
        if (param3.empty() == false) {funcKey.push_back(param3);}
        auto functionKey  = QemuAnswer::createFunctionKey(funcKey);
        answer = qemu_returns->find(functionKey);
#if defined(PRINT_DATA_STRUCTURES) // set in development environment only
        if (answer != nullptr)
        {
            printf("Dbus::qemuAnswer(): found key %s\n", functionKey.c_str());
        }
#endif
    }
    return answer;
}


} // namespace util
} // namespace ipmi
