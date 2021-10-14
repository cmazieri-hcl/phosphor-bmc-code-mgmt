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

#include <ipmid/utils.hpp>

namespace ipmi
{

using PropertyStringMap = std::map<std::string, std::string>;
using ObjectPropertyStringValue = std::map<std::string, PropertyStringMap>;

namespace util
{

struct ObjectDataSearchable
{
    std::vector<std::string> searchObject(const std::string& propSearch,
                                          const std::string& valueSearch) const;
    std::string getValue(const ipmi::DbusObjectPath& object,
                         const std::string& property) const;
    ObjectDataSearchable& operator=(const ObjectDataSearchable&) = delete;
    ObjectPropertyStringValue objectData;
};

std::string variantToString(const ipmi::Value & variantVar);

void printObjectPropertyStringValue(const ObjectPropertyStringValue& tree);

void copyPropertyMapToPropertyStringMap(const PropertyMap& valuesProp,
                                        PropertyStringMap *strProp);

bool vectorContainsString(const std::vector<std::string>& array,
                          const std::string& str);


} // namespace util
} // namespace ipmi
