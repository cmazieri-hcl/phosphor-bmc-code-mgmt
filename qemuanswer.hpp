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

#include <vector>
#include <map>
#include <memory>
#include <string>

/**
 * @brief The QemuAnswer intends to help emulating results inside QEMU emulator
 *
 *  It stores text lines indicating results mapped as key for function names
 *    and their parameters.
 *  These lines can be used for calling functions to build an emulated
 *    result when running inside the QEMU emulator.
 *  The calling function is responsible to creating its results from the text
 *    lines.
 *  The QemuAnswer receives a text file as parameter stored anywhere in the
 *    emulator machine.
 *
 *  To distinguish between running in the emulator and running in a real
 *    hardware it is necessary to define a environment variable in the image
 *    the file '/etc/profile' can have:
 * @code
 *        OPENBMC_QEMU_EMULATOR_DEVEL=1
 *        export OPENBMC_QEMU_EMULATOR_DEVEL
 * @endcode
 *
 * Answer file example where lines starting with '#' are comments and strings
 *   between [] are function key information:
 * @code
 *   ## it is the key:  function parameter1 parameter2 ...
 *   [getObjectTree xyz.openbmc_project.Software.BMC.Updater]
 *   #level #path, lines are stored and mapped as answer for call above
 *      0    /xyz/openbmc_project/software
 *      1    /xyz/openbmc_project/software/1a56bff3
 *      2    /xyz/openbmc_project/software/1a56bff3/cpld
 *      3    /xyz/openbmc_project/software/1a56bff3/cpld/1
 *      3    /xyz/openbmc_project/software/1a56bff3/cpld/3
 *
 *  [otherFunction parameter1 parameter2]
 *  line1string1 string2
 *  line2string1 string2
 *  line3string1 string2
 * @endcode
 *
 * Code example using the file above as /home/root/Busctl.txt inside the image:
 * @code
 *    class Busctl
 *    {
 *       private:
 *          AnswerData  * _qemuAnswer = nullptr;
 *       public:
 *          Busctl(): _qemuAnswer(new AnswerData("/home/root/Busctl.txt")
 *          {
 *              // Empty
 *          }
 *          ObjectTree getObjectTree(const std::string& service)
 *          {
 *              ObjectTree tree;  // ObjectTree is just a type here as example
 *              if (AnswerData::isEmulatorInstanceQEMU() == true)
 *              {
 *                 std::vector<std::string> key{__FUNCTION__};
 *                 key.push_back(service);
 *                 // key has function name and the service
 *                 auto answer =
 *                      _qemuAnswer.find(QemuAnswer::createFunctionKey(key);
 *                 // the data will be available when calling
 *                 // getObjectTree("xyz.openbmc_project.Software.BMC.Updater")
 *                 if (answer != nullptr) // there is answer for that call
 *                 {
 *                    char buffer[2048];
 *                    int  value = 0;
 *                    // parse lines to populate 'tree'
 *                    for (const std::string& line : answer->lines_data)
 *                    {
 *                       if (sscanf(line.c_str(), "%d %s", &value, buffer) == 2)
 *                       {
 *                          // supposing ObjectTree::insert(int, char*) exists
 *                          tree.insert(level, buffer);
 *                       }
 *                    }
 *                    // return emulated data
 *                    return tree;
 *                 }
 *              }
 *
 *              // perform normal function, supposing not running in Qemu
 *              ...
 *              return tree;
 *          }
 *    }
 * @endcode
 */
class QemuAnswer
{
 public:
    struct AnswerData
    {
        std::vector<std::string> lines_data;
    };
    /**
     * @brief QemuAnswer the contructor
     * @param filename full pathname of the file that contains answers
     *                 this file will be parsed and its answers will be
     *                 stored in _qemu_emulator_answers
     */
    explicit QemuAnswer(const std::string& filename);
    virtual ~QemuAnswer();

    /** @brief builds a function key which MUST be the first element
     *            in the vector, other elements are function parameters
     */
    static std::string createFunctionKey(const std::vector<std::string>& vect);

    /** @brief returns true if the environment variable
     *           OPENBMC_QEMU_EMULATOR_DEVEL  exists
     */
    static bool isEmulatorInstanceQEMU();


    /** find() static method receiving an object and checking it it is valid
     *
     */
    static const QemuAnswer::AnswerData * find(QemuAnswer *answerObj,
                                               const std::string& func,
                                               const std::string& param1 = {},
                                               const std::string& param2 = {},
                                               const std::string& param3 = {});

    /** @brief returns true when there is no answer stored*/
    bool        isEmpty() const;

    /** @brief returns an answer if present in file passed by the constructor
     *  @param functionkey must be created by @sa createFunctionKey()
     */
    const AnswerData * find(const std::string& functionkey) const;

 private:
    static std::string createFunctionKey(const std::string& descriptionLine);
    void parseQemuAnswerFile(const std::string& filename);
 private:
    std::map<std::string, std::unique_ptr<AnswerData>>
                          _qemu_emulator_answers;
    static  bool          _qemu_instance;
};
