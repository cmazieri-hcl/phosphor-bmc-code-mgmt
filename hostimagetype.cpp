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

#include "hostimagetype.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <regex>
#include <boost/algorithm/string.hpp>

#define  READ_BUFFER_SIZE   1024
#define  LINE_BUFFER_SIZE   512
#define  MAX_LINES          512

namespace phosphor
{
namespace software
{
namespace updater
{

/**
 *   These are the types handled so far, they must match with enumerator @sa Type
 **/
const std::vector<std::string>  HostImageType::m_types{"bios", "cpld", "bic", "vr", "me"};

HostImageType::HostImageType(const std::string &imageDirectory)
    : m_imageTypeId(Unknown)
{
    if (std::filesystem::is_directory(imageDirectory) == true)
    {
        std::vector<std::string> binary_files;
        std::vector<std::string> text_files;
        for(auto& file: std::filesystem::directory_iterator(imageDirectory))
        {   // check if file name does not start with "MANIFEST"
            if (file.path().filename().string().rfind("MANIFEST", 0) != 0)
            {
                auto contentType = fileContent(file.path());
                if (contentType == FileContent::Text) {
                    text_files.push_back(file.path());
                }
                else if (contentType == FileContent::Binary) {
                    binary_files.push_back(file.path());
                }
            }
        }
        // from binary files guess the image file (greater file in size)
        gessImageName(binary_files);
        // guess image type, for name try binary files first
        guessTypeByName(binary_files)
                || guessTypeByName(text_files)
                || guessTypeByContent(text_files);
    }
}

HostImageType::~HostImageType()
{
    // empty destructor
}

std::string HostImageType::type(Type id)
{
    std::string ret;
    if (id != Unknown)
    {
        ret =  HostImageType::m_types.at(id);
    }
    return ret;
}

std::vector<std::string> HostImageType::availableTypes()
{
    return HostImageType::m_types;
}

std::string HostImageType::curTypeString() const
{
    return HostImageType::type(m_imageTypeId);
}

HostImageType::Type HostImageType::curType() const
{
    return m_imageTypeId;
}

std::string HostImageType::imageFile() const
{
    return m_imageFile;
}

bool HostImageType::guessTypeByName(const std::vector<std::string> &files)
{
    bool gotType = false;
    auto fileCounter = files.size();
    while (fileCounter-- && gotType == false)
    {
           auto name  = std::filesystem::path(files.at(fileCounter)).filename().string();
           for (size_t counter = 0; counter < name.size(); ++counter)
           {   // an image name such as "bios.bin" will became "bios bin"
               if (name[counter] == '_' || name[counter] == '-' || name[counter] == '.')
               {
                   name.replace(counter, 1, " ");
               }
           }
           for (size_t counter=0; counter < HostImageType::m_types.size(); ++counter)
           {
               std::string type_lowercase_plus_space = HostImageType::m_types.at(counter) + ' ';
               /* convert file name to lowercase as the type names are in lowercase
                *   search in name for strings such as "bios " or "cpld " (having space at end)
                */
               std::string name_lowercase = name;
               boost::to_lower(name);
               if (name_lowercase.find(type_lowercase_plus_space) != std::string::npos )
               {
                  m_imageTypeId = static_cast<Type>(counter);
                  gotType = true;
                  break;
               }
           }
    }
    return gotType;
}

bool HostImageType::guessTypeByContent(const std::vector<std::string> &files)
{
    bool gotType = false;
    for (size_t counterType = 0;
         gotType == false  && counterType < HostImageType::m_types.size();
         ++counterType)
    {
        auto  upper_type = HostImageType::m_types.at(counterType);
        boost::to_upper(upper_type);
        const std::string regStr = "^" + upper_type
                                       + "$|\\s"
                                       + upper_type
                                       + "\\s";
        const std::regex reg(regStr);
        auto fileCounter = files.size();
        while (fileCounter-- && gotType == false)
        {
            std::ifstream stream(files.at(fileCounter), std::ios_base::in);
            if (stream.is_open() == true)
            {
                char line[LINE_BUFFER_SIZE];
                for(int counter=0; counter < MAX_LINES && stream.good(); ++counter)
                {
                    stream.getline(line, sizeof(line));
                    if (std::regex_search(line, reg) == true)
                    {
                        m_imageTypeId = static_cast<Type>(counterType);
                        gotType = true;
                        break;
                    }
                }
            }
            stream.close();
        }
    }
    return gotType;
}

void HostImageType::gessImageName(const std::vector<std::string> &files)
{
    if (files.empty() == false)
    {
        auto greaterIndex = 0;
        auto size = std::filesystem::file_size(files.at(0));
        for (size_t counter=1; counter < files.size(); ++counter)
        {
            if (std::filesystem::file_size(files.at(counter)) > size)
            {
                greaterIndex = counter;
            }
        }
        m_imageFile = files.at(greaterIndex);
    }
}

bool HostImageType::fileIsBinary(const std::string &filename) const
{
     return fileContent(filename) == HostImageType::Binary;
}

bool HostImageType::fileIsText(const std::string &filename) const
{
    return fileContent(filename) == HostImageType::Text;
}

HostImageType::FileContent
HostImageType::fileContent(const std::string &filename) const
{
    HostImageType::FileContent content = HostImageType::NotRegular;
    if (std::filesystem::is_regular_file(filename))
    {
        std::ifstream stream(filename.c_str(), std::ios_base::in | std::ios_base::binary);
        if (stream.is_open() == true)
        {
            char buffer[READ_BUFFER_SIZE];
            stream.read(buffer, sizeof(buffer));
            auto size  = stream.gcount();
            while (size-- > 0)
            {
                if (buffer[size] == 0x00)
                {
                    content = HostImageType::Binary;
                    break;
                }
            }
            stream.close();
            // if it is still NotRegular change to Text
            if (content ==  HostImageType::NotRegular)
            {
                content =  HostImageType::Text;
            }
        }
    }
    return content;
}


} // namespace updater
} // namespace software
} // namespace phosphor

