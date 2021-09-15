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

private:
    void createImageDirectories()
    {
        _bios_dir = _directory + '/'+ "bios";
        _cpld_dir  = _directory + '/'+ "cpld";
        _me_dir   = _directory + '/'+ "me";
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
    HostImageType imagedir(_directory);
    ASSERT_EQ(HostImageType::Unknown, imagedir.curType());
    ASSERT_EQ(imagedir.imageFile().size(), 0);
    ASSERT_EQ(imagedir.curTypeString().size(), 0);
}


TEST_F(HostImageTypeTest, TestDirectoryDoesNotExist)
{
    HostImageType imagedir("_this_directory_MUST_not_exist_zzzzzzzzzz00000000");
    ASSERT_EQ(HostImageType::Unknown, imagedir.curType());
    ASSERT_EQ(imagedir.imageFile().size(), 0);
    ASSERT_EQ(imagedir.curTypeString().size(), 0);
}

TEST_F(HostImageTypeTest, TestUnknownImage)
{
    HostImageType imagedirNone(_none_dir);
    ASSERT_EQ(HostImageType::Unknown, imagedirNone.curType());
    ASSERT_EQ(imagedirNone.imageFile().size(), 0);
    ASSERT_EQ(imagedirNone.curTypeString().size(), 0);
}

TEST_F(HostImageTypeTest, TestBiosImageByName)
{
    HostImageType bios_image(_bios_dir);
    ASSERT_EQ("bios", bios_image.curTypeString());
    ASSERT_EQ(HostImageType::BIOS, bios_image.curType());
    ASSERT_NE(bios_image.imageFile().find("bios.bin"), std::string::npos);
}

TEST_F(HostImageTypeTest, TestCpldImageByName)
{
    HostImageType cpld_image(_cpld_dir);
    ASSERT_EQ("cpld", cpld_image.curTypeString());
    ASSERT_EQ(HostImageType::CPLD, cpld_image.curType());
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

    HostImageType cpld_image(_cpld_dir);
    ASSERT_EQ("cpld", cpld_image.curTypeString());
    ASSERT_EQ(HostImageType::CPLD, cpld_image.curType());
}

TEST_F(HostImageTypeTest,  ImageTypesArrayNotEmpty)
{
#ifdef HOST_FIRMWARE_UPGRADE
    auto size = HostImageType::availableTypes().size();
    ASSERT_NE(size, 0);
#ifdef HOST_BIOS_UPGRADE
    auto exists = check_vector_strings(HostImageType::availableTypes(), "bios");
    ASSERT_TRUE(exists);
#endif
#endif
}

TEST_F(HostImageTypeTest, TestMeImageByContent)
{
     HostImageType me_image(_me_dir);
     ASSERT_EQ("me", me_image.curTypeString());
     ASSERT_EQ(HostImageType::ME, me_image.curType());
     const std::string image_name{"YMM05_FwOrch_1550736000.bin"};
     ASSERT_TRUE(me_image.imageFile().find(image_name) != std::string::npos);
}
