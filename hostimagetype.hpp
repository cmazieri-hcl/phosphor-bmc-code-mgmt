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

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace updater
{

/**
 * @brief The HostImageType class aims to identify an 'Image Type'
 *        of the firmware being updated into a BMC controlled Host.
 *
 * Static methods:
 * @li  @sa availableTypes() the list of available types being handled by BMC
 * @li  @sa type(Type id) a string id for a such Type id
 *
 * By passing the image directory into the constructor, this class provides:
 *   @li The current type of an image directory @sa curType() and @sa curTypeString()
 *
 */
class HostImageType
{
public:
    enum Type
    {
        Unknown = -1,
        BIOS    = 0,
        CPLD,
        BIC,
        VR,
        ME
    };   
    explicit HostImageType(const std::string &imageDirectory);
    HostImageType()=delete;
    virtual ~HostImageType();
public:
    static std::string  type(Type id);
    static std::vector<std::string> availableTypes();
public:
    std::string         curTypeString() const;
    Type                curType() const;
    std::string         imageFile() const;
private:
    enum FileContent
    {
        NotRegular,
        Text,
        Binary,
    };
    /**
     * @brief guessTypeByName() checks if the filenames contain a defined ''Image Type' with a separator
     * 
     *      For example a filename like 'image-cpld.bin' contains the string 'cpld' between separators '-' and '.'
     * 
     * @param files   files the list of text file to search
     * @return  true when found such string type, otherwise false
     */
    bool                guessTypeByName(const std::vector<std::string>& files);
    /**
     * @brief guessTypeByContent() searches for the string type isolated in uppercase.
     *
     * If a text file present in the image directory contains
     *    for instance the word "BIOS" it is considered a image type bios.
     * 
     * @param files the list of text file to search
     * @return true when found such string type, otherwise false
     */
    bool                guessTypeByContent(const std::vector<std::string>& files);
    void                gessImageName(const std::vector<std::string>& files);
    bool                fileIsBinary(const std::string& filename) const;
    bool                fileIsText(const std::string& filename)   const;
    HostImageType::FileContent fileContent(const std::string& filename)  const;   
private: 
   static const std::vector<std::string>  m_types;
   Type                 m_imageTypeId;
   std::string          m_imageFile;   // binary image file
};


} // namespace updater
} // namespace software
} // namespace phosphor
