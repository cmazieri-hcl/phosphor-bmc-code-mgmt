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
#include "ipmiutils.hpp"
#include <boost/algorithm/string.hpp>

namespace ipmi
{
namespace util
{


std::string variantToString(const Value &variantVar)
{
    std::string value;
    if (std::holds_alternative<std::string>(variantVar))
    {
        value = std::get<std::string>(variantVar);
    }
    else if (std::holds_alternative<bool>(variantVar))
    {
        auto boolean = std::get<bool>(variantVar);
        value = boolean == true ? "true" : "false";
    }
    else if (std::holds_alternative<uint8_t>(variantVar))
    {
        value = std::to_string(std::get<uint8_t>(variantVar));
    }
    else if (std::holds_alternative<int16_t>(variantVar))
    {
        value = std::to_string(std::get<int16_t>(variantVar));
    }
    else if (std::holds_alternative<uint16_t>(variantVar))
    {
        value = std::to_string( std::get<uint16_t>(variantVar));
    }
    else if (std::holds_alternative<int32_t>(variantVar))
    {
        value = std::to_string(std::get<int32_t>(variantVar));
    }
    else if (std::holds_alternative<uint32_t>(variantVar))
    {
        value = std::to_string(std::get<uint32_t>(variantVar));
    }
    else if (std::holds_alternative<int64_t>(variantVar))
    {
        value = std::to_string(std::get<int64_t>(variantVar));
    }
    else if (std::holds_alternative<uint64_t>(variantVar))
    {
        value = std::to_string(std::get<uint64_t>(variantVar));
    }
    else if (std::holds_alternative<double>(variantVar))
    {
        value = std::to_string(std::get<double>(variantVar));
    }
    return value;
}


std::vector<std::string>
ObjectDataSearchable::searchObject(const std::string &propSearch,
                                   const std::string &valueSearch) const
{
    std::vector<DbusObjectPath> objectPath;
    for (auto & obj : this->objectData)
    {
        for (auto & propMap : obj.second)
        {
            const std::string& propertyName  = propMap.first;
            const std::string& propertyValue = propMap.second;
            if (boost::iequals(propSearch, propertyName)
                    && boost::iequals(valueSearch, propertyValue)
                    && vectorContainsString(objectPath, obj.first) == false)
            {
               objectPath.push_back(obj.first);
            }
        }
    }
    // try again using substr for Property name, but value MUST match
    if (objectPath.empty() == true)
    {
        for (auto & obj : this->objectData)
        {
            for (auto & propMap : obj.second)
            {
                const std::string& propertyName  = propMap.first;
                const std::string& propertyValue = propMap.second;
                if (boost::algorithm::icontains(propertyName, propSearch)
                        && boost::iequals(propertyValue, valueSearch)
                        && vectorContainsString(objectPath, obj.first) == false)
                {
                    objectPath.push_back(obj.first);
                }
            }
        }
    }
    return objectPath;
}


std::string
ObjectDataSearchable::getValue(const DbusObjectPath &object,
                               const std::string &property) const
{
    for (auto & obj : this->objectData)
    {
        if (boost::iequals(object, obj.first) == true)
        {
            for (auto & propMap : obj.second)
            {
                if (boost::iequals(property, propMap.first))
                {
                    return propMap.second;
                }
            }
        }
    }
    return std::string {};
}


void printObjectPropertyStringValue(const ObjectPropertyStringValue &tree)
{
    auto counter = 0;
    for (auto & item : tree)
    {
        std::string subpath = item.first;
        printf("\n  #[%d/%u] object path=%s\n", ++counter,  tree.size(),
               subpath.c_str());
        printf("  %s\n", subpath.c_str());
        for (auto &prop : item.second)
        {
            std::string property = prop.first;
            std::string value = prop.second;
            printf("\t%-30s = %s\n ",
                   property.c_str(), value.c_str());
        }
    }
}


void copyPropertyMapToPropertyStringMap(const PropertyMap &valuesProp,
                                        PropertyStringMap *strProp)
{
    for (auto& prop : valuesProp)
    {
        std::string name = prop.first;
        Value val = prop.second;
        std::string value = variantToString(val);
        (*strProp)[name] = value;
    }
}


bool vectorContainsString(const std::vector<std::string> &array,
                          const std::string &str)
{
    for (const auto & item: array)
    {
        if (item == str)
        {
            return true;
        }
    }
    return false;
}


} // namespace util
} // namespace ipmi
