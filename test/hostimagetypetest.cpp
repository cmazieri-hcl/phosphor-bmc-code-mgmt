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

#include "hostimagetype.hpp"


#include <filesystem>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>


using namespace phosphor::software::updater;

#include "imagetype_host_association.hpp"

/* just to use protected members  */
class TestImagetypeHostsAssociation : public ImagetypeHostsAssociation
{
 public:
    TestImagetypeHostsAssociation(sdbusplus::bus::bus& bus,
                                  const std::string& imgPath)
        : ImagetypeHostsAssociation(bus)
    {
        setData(imgPath);
    }
    EntityManagerDict getActiveHostsFromEntityManager()
    {
        return ImagetypeHostsAssociation::getActiveHostsFromEntityManager();
    }
    ImageTypeList createImageTypeList()
    {
        auto dict = getActiveHostsFromEntityManager();
        return ImagetypeHostsAssociation::createImageTypeList(dict);
    }
    ImageTypeList createImageTypeList(EntityManagerDict& dict)
    {
         return ImagetypeHostsAssociation::createImageTypeList(dict);
    }
    bool identifyImageType(const ImageTypeList & list)
    {
        return ImagetypeHostsAssociation::identifyImageType(list);
    }
    void removeHostsImageTypeNotIn(EntityManagerDict *dit)
    {
        return ImagetypeHostsAssociation::removeHostsImageTypeNotIn(dit);
    }
};





class HostImageTypeTest : public testing::Test
{
  protected:
    virtual void SetUp()
    {
        char imagesdir[] = ".images_zzzzXXXXXX";
        _directory = mkdtemp(imagesdir);

        createImageDirectories();
        if (_directory.empty())
        {
            throw std::bad_alloc();
        }
        createBiosFiles();
        createNoneFiles();
        createCpldFiles();
        createMeFiles();
    }

    virtual void TearDown()
    {
        std::filesystem::remove_all(_directory);
    }

    bool check_vector_strings(const std::vector<std::string>& array,
                              const std::string& value)
    {
        std::string upper_value = value;
        std::string lower_value = value;
        boost::to_upper(upper_value);
        boost::to_lower(lower_value);
        bool ret = false;
        auto counter = array.size();
        while (counter-- > 0)
        {
            if (array.at(counter) == upper_value
                    || array.at(counter) == lower_value)
            {
                ret = true;
                break;
            }
        }
        return ret;
    }
    std::string _directory;
    std::string _bios_dir;
    std::string _cpld_dir;
    std::string _me_dir;
    std::string _none_dir;
    std::vector<std::string> _image_types_array = {"BIOS", "cpld", "Me", "vr"};
    sdbusplus::bus::bus  _bus = sdbusplus::bus::new_default();

 private:
    void createImageDirectories()
    {
        _bios_dir  = _directory + '/' + "bios";
        _cpld_dir  = _directory + '/' + "cpld";
        _me_dir    = _directory + '/' + "me";
        _none_dir  = _directory + '/' + "none";
        mkdir(_bios_dir.c_str(), 0775);
        mkdir(_cpld_dir.c_str(), 0775);
        mkdir(_me_dir.c_str(), 0775);
        mkdir(_none_dir.c_str(), 0775);
    }

    void createBinaryFile(const std::string& filename,
                          const char *data = nullptr,
                          int len = 0)
    {
        int  file = ::open(filename.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
        ASSERT_GT(file, 0);
        if (data == nullptr)
        {
            char binary_content [] = "1g\nnull=\0\0\0\nlast line\n";
            data = binary_content;
            len  = sizeof(data);
        }
        ASSERT_EQ(::write(file, data, len), len);
        ::close(file);
    }

    void createTextFile(const std::string& filename, const char *data = nullptr)
    {
        std::ofstream file;
        file.open(filename, std::ofstream::out);
        ASSERT_TRUE(file.is_open());
        if (data == nullptr)
        {
            data = "This is a simple text file\n"
                   "abcdsfzsredtbdfdvdvvv 1234354565";
        }
        file << data;
        file.close();
        ASSERT_GT(std::filesystem::file_size(filename), 0);
    }

    void createManifest(const std::string& filename)
    {
        const char *data =
           "purpose=xyz.openbmc_project.Software.Version.VersionPurpose.Host\n"
           "version=1.0mz-dev\n"
           "MachineName=yosemitv2\n"
           "KeyType=OpenBMC\n"
           "HashType=RSA-SHA256\n";
        createTextFile(filename, data);
    }

    void createBiosFiles()
    {
        createManifest(_bios_dir  + '/' + "MANIFEST");
        char bios_image [] = "This is the image, null=\0\0\n/nData is binary";
        createBinaryFile(_bios_dir  + '/' + "bios.bin", bios_image,
                         sizeof(bios_image));
        createTextFile(_bios_dir  + '/' + "bios.bin.sig");
        createTextFile(_bios_dir  + '/' + "MANIFEST.sig");
        createTextFile(_bios_dir  + '/' + "publickey");
        createTextFile(_bios_dir  + '/' + "publickey.sig");
    }

    void createNoneFiles()
    {
         createManifest(_none_dir  + '/' + "MANIFEST");
         char image [] = "This is the image, null=\0\0\n/nData is binary";
         createBinaryFile(_none_dir  + '/' + "none.bin", image, sizeof(image));
         createTextFile(_none_dir  + '/' + "publickey");
         createTextFile(_none_dir  + '/' + "publickey.sig");
    }

    void createCpldFiles()
    {
        createManifest(_cpld_dir  + '/' + "MANIFEST");
        char image [] = "This is the image, null=\0\0\n/nData is binary";
        createBinaryFile(_cpld_dir
                         + '/' + "Twin_Lakes_DVT_0811_1.01_0X51020101_OBMC.bin",
                         image, sizeof(image));
        createTextFile(_cpld_dir
                       + '/' + "Twin_Lakes_DVT_0811_1.01_0X51020101.jed");
        char cpld_note[] =
    "===============================================================\n"
    "    F09 CPLD code v1.01 release note\n"
    "===============================================================\n"
    "Release date : 2017/8/11\n"
    "File name : Twin_Lakes_DVT_0811_1.01_0X51020101.jed "
    "(update with Lattice dongle)\n"
    "            Twin_Lakes_DVT_0811_1.01_0X51020101_OBMC.bin "
    "(update with yafuflash)\n"
    "Checksum :  0x8C28\n"
    "Usercode :  0x51020101\n"
    "Compatible firmware version : Firmware version above Bridge IC FW 1.08 "
    "and openBMC fby2-v2.0.\n"
    "Remark : Based on Yuba City CPLD code (YubaCity_rev0p8_External_0221)\n"
    "\n"
    "Change item :\n"
    "Rev 1.01\n"
    "1. Add a feature to disable PVNN_PCH_STBY and P1V05_PCH_STBY VR before "
    "VR firmware update by FM_DISABLE_PCH_VR. \n"
    "\n"
    "Rev 1.00\n"
    "1. PWRGD_CPU0_LVC3_R will keep low until PWRGD_PCH_PWROK is valid "
    "to prevernt a fake SOC temperature event. \n"
    "2. Any glitch on RST_PLTRST_N will be filtered out until "
    "PWRGD_SYS_PWROK.\n"
    "3. Fix PVCCIN power down sequence issue. (VCCIO failure to 0.65V to "
    "VCCIN 1.2V <= 0.5ms)\n"
    "4. Support CPLD on-line update via I2C bus.\n"
    "5. Assign FAST_THROTTLE_N to throttle SOC when HSC OCP event.\n"
    "\n"
    "Rev 0.01\n"
    "1. Initial release for power-on.\n"
                "\n";
        createTextFile(_cpld_dir    + '/' + "09_CPLD_Note_0811_Rev1.01.txt",
                       cpld_note);
    }

    void createMeFiles()
    {
        createManifest(_me_dir  + '/' + "MANIFEST");
        char image_elf [] =
          {
            0x45, 0x7f, 0x46, 0x4c, 0x01, 0x02, 0x00, 0x01,
            0x00, 0x3e, 0x01, 0x2a, 0x40, 0x00, 0x00, 0x01
          };
        createBinaryFile(_me_dir  + '/' + "afulnx_64_twinlakes",
                         image_elf, sizeof(image_elf));
        char image [] = "This is the image,"
                        "PWRGD_CPU0_LVC3_R will keep low until PWRGD_PCH_PWROK"
                        " is valid to null=\0\0\n/nData is binary";
        createBinaryFile(_me_dir    + '/' + "YMM05_FwOrch_1550736000.bin",
                         image, sizeof(image));
        char script_data[] = "#!/usr/bin/env bash\n"
  "  RCUTIMEOUT=$(cat /sys/module/rcupdate/parameters/rcu_cpu_stall_timeout)\n"
  "# Try to reset ME first to prevent ME region locking issue\n"
  "# this ipmi command reboots ME without rebooting host paltform\n"
  "# Try to flash ME region first and then proceed with the rest\n"
  "  retry_command ./afulnx_64_twinlakes YMM05_FwOrch_1550736000.bin /ME\n"
  "echo \"ME region flash failed after 3 retries: ${me_flash_e_code}\"\n"
  "echo \"$RCUTIMEOUT\" >"
  " /sys/module/rcupdate/parameters/rcu_cpu_stall_timeout\n";

        createTextFile(_me_dir    + '/' + "run_script", script_data);
    }
};

TEST_F(HostImageTypeTest, TestDirectoryDoesContainImage)
{
    HostImageType imagedir(_directory, _image_types_array);
    ASSERT_EQ(HostImageType::Unknown, imagedir.curType());
    ASSERT_EQ(imagedir.imageFile().size(), 0);
    ASSERT_EQ(imagedir.curTypeString().size(), 0);
}


TEST_F(HostImageTypeTest, TestDirectoryDoesNotExist)
{
    HostImageType imagedir("_this_directory_MUST_not_exist_zzzzzzzzzz00000000",
                           _image_types_array);
    ASSERT_EQ(HostImageType::Unknown, imagedir.curType());
    ASSERT_EQ(imagedir.imageFile().size(), 0);
    ASSERT_EQ(imagedir.curTypeString().size(), 0);
}

TEST_F(HostImageTypeTest, TestUnknownImage)
{
    HostImageType imagedirNone(_none_dir, _image_types_array);
    ASSERT_EQ(HostImageType::Unknown, imagedirNone.curType());
    ASSERT_EQ(imagedirNone.imageFile().size(), 0);
    ASSERT_EQ(imagedirNone.curTypeString().size(), 0);
}

TEST_F(HostImageTypeTest, TestBiosImageByName)
{
    HostImageType bios_image(_bios_dir, _image_types_array);
    ASSERT_EQ("bios", bios_image.curTypeString());
    ASSERT_NE(bios_image.imageFile().find("bios.bin"), std::string::npos);
}

TEST_F(HostImageTypeTest, TestCpldImageByName)
{
    HostImageType cpld_image(_cpld_dir, _image_types_array);
    ASSERT_EQ("cpld", cpld_image.curTypeString());
    const std::string
            image_name{"Twin_Lakes_DVT_0811_1.01_0X51020101_OBMC.bin"};
    ASSERT_NE(cpld_image.imageFile().find(image_name), std::string::npos);
}


TEST_F(HostImageTypeTest, TestCpldImageByContent)
{
    const std::string cpld_release_notes    = _cpld_dir
                                       + '/' + "09_CPLD_Note_0811_Rev1.01.txt";
    const std::string renamed_release_notes = _cpld_dir
                                              + '/' + "guess_content.txt";
    // rename the file to avoid guessing Type by file name
    ASSERT_TRUE(std::filesystem::exists(cpld_release_notes));
    ASSERT_FALSE(std::filesystem::exists(renamed_release_notes));
    std::filesystem::rename(cpld_release_notes, renamed_release_notes);
    ASSERT_FALSE(std::filesystem::exists(cpld_release_notes));
    ASSERT_TRUE(std::filesystem::exists(renamed_release_notes));

    HostImageType cpld_image(_cpld_dir, _image_types_array);
    ASSERT_EQ("cpld", cpld_image.curTypeString());
}


TEST_F(HostImageTypeTest, TestMeImageByContent)
{
     HostImageType me_image(_me_dir, _image_types_array);
     ASSERT_EQ("me", me_image.curTypeString());
     const std::string image_name{"YMM05_FwOrch_1550736000.bin"};
     ASSERT_TRUE(me_image.imageFile().find(image_name) != std::string::npos);
}


TEST_F(HostImageTypeTest, TestHostsAssociation_getActiveHostsFromEntityManager)
{
    TestImagetypeHostsAssociation hostsAssoc(_bus, _cpld_dir);
    auto entity_manager_dic = hostsAssoc.getActiveHostsFromEntityManager();

    ASSERT_EQ(entity_manager_dic.empty(), false);
}


TEST_F(HostImageTypeTest, TestHostsAssociation_createImageTypeList)
{
    TestImagetypeHostsAssociation hostsAssoc(_bus, _cpld_dir);
    auto global_image_list = hostsAssoc.createImageTypeList();

    ASSERT_EQ(global_image_list.empty(), false);
}


TEST_F(HostImageTypeTest, TestHostsAssociation_identifyImageType_cpld)
{
    TestImagetypeHostsAssociation hostsAssoc(_bus, _cpld_dir);
    auto global_image_list = hostsAssoc.createImageTypeList();

    ASSERT_EQ(global_image_list.empty(), false);

    auto ok = hostsAssoc.identifyImageType(global_image_list);
    ASSERT_EQ(ok, true);

    ASSERT_EQ(hostsAssoc.imageType(), "cpld");
}


TEST_F(HostImageTypeTest, TestHostsAssociation_identifyImageType_bios)
{
    TestImagetypeHostsAssociation hostsAssoc(_bus, _bios_dir);
    auto global_image_list = hostsAssoc.createImageTypeList();

    ASSERT_EQ(global_image_list.empty(), false);

    auto ok = hostsAssoc.identifyImageType(global_image_list);
    ASSERT_EQ(ok, true);

    ASSERT_EQ(hostsAssoc.imageType(), "bios");
}



TEST_F(HostImageTypeTest, TestHostsAssociation_identifyImageType_Manifest_file)
{
    std::string manifest_file{_none_dir};
    manifest_file += "/MANIFEST";
    FILE *manifest_stream = fopen(manifest_file.c_str(), "a+");
    ASSERT_NE(manifest_stream, nullptr);
    auto written = fprintf(manifest_stream, "\nImageType=Vr\n");
    fclose(manifest_stream);
    ASSERT_NE(written, 0);

    TestImagetypeHostsAssociation hostsAssoc(_bus, _none_dir);
    auto global_image_list = hostsAssoc.createImageTypeList();

    ASSERT_EQ(global_image_list.empty(), false);

    auto ok = hostsAssoc.identifyImageType(global_image_list);
    ASSERT_EQ(ok, true);

    ASSERT_EQ(hostsAssoc.imageType(), "vr");
}


TEST_F(HostImageTypeTest, TestHostsAssociation_removeHostsImageTypeNotIn)
{
    TestImagetypeHostsAssociation hostsAssoc(_bus, _cpld_dir);
    auto entity_manager_dic = hostsAssoc.getActiveHostsFromEntityManager();
    ASSERT_EQ(entity_manager_dic.empty(), false);

    auto global_image_list = hostsAssoc.createImageTypeList();
    ASSERT_EQ(global_image_list.empty(), false);

    std::string manifest_file{_cpld_dir};
    manifest_file += "/MANIFEST";
    FILE *manifest_stream = fopen(manifest_file.c_str(), "a+");
    ASSERT_NE(manifest_stream, nullptr);
    auto written = fprintf(manifest_stream, "\nImageType=Vr\n");
    fclose(manifest_stream);
    ASSERT_NE(written, 0);

    auto ok = hostsAssoc.identifyImageType(global_image_list);
    ASSERT_EQ(ok, true);
    ASSERT_EQ(hostsAssoc.imageType(), "vr");

    ASSERT_FALSE(entity_manager_dic.empty());
    hostsAssoc.removeHostsImageTypeNotIn(&entity_manager_dic);
    ASSERT_TRUE(entity_manager_dic.empty());
}

