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

#include "qemuanswer.hpp"
#include <boost/algorithm/string.hpp>
#include <fstream>

/** For qemu images define this environment variable */
bool QemuAnswer::_qemu_instance =
        ::secure_getenv("OPENBMC_QEMU_EMULATOR_DEVEL") != nullptr;


QemuAnswer::QemuAnswer(const std::string& filename)
{
    parseQemuAnswerFile(filename);
}


QemuAnswer::~QemuAnswer()
{
    // Empty
}


std::string
QemuAnswer::createFunctionKey(const std::string &descriptionLine)
{
     std::string line(descriptionLine);
     line.erase(line.size() -1, 1);
     line.erase(0, 1);
     std::vector<std::string> vect;
     boost::algorithm::split(vect, line, boost::algorithm::is_space());
     return createFunctionKey(vect);
}


std::string
QemuAnswer::createFunctionKey(const std::vector<std::string> &vect)
{
    std::string key;
    for (const std::string& item: vect)
    {
        if (item.size() > 0 && item.at(0) != ' ')
        {
            if (key.size() != 0)
            {
                key +='%';
            }
            key += item;
        }
    }
    return key;
}


bool QemuAnswer::isEmulatorInstanceQEMU()
{
    return QemuAnswer::_qemu_instance;
}


const QemuAnswer::AnswerData *
QemuAnswer::find(const std::string &functionkey) const
{
    const QemuAnswer::AnswerData *data = nullptr;
    auto answer = _qemu_emulator_answers.find(functionkey);
    if (answer != _qemu_emulator_answers.end())
    {
        data = const_cast<const AnswerData *> (answer->second.get());
    }
    return data;
}


const QemuAnswer::AnswerData *
QemuAnswer::find(QemuAnswer *answerObj,
                 const std::string &func,
                 const std::string &param1,
                 const std::string &param2,
                 const std::string &param3)
{
    const QemuAnswer::AnswerData *answer = nullptr;
    if (answerObj != nullptr)
    {
        std::vector<std::string> funcKey{func};
        if (param1.empty() == false) {funcKey.push_back(param1);}
        if (param2.empty() == false) {funcKey.push_back(param2);}
        if (param3.empty() == false) {funcKey.push_back(param3);}
        answer = answerObj->find(QemuAnswer::createFunctionKey(funcKey));
    }
    return answer;
}


bool QemuAnswer::isEmpty() const
{
    return _qemu_emulator_answers.empty();
}


void QemuAnswer::parseQemuAnswerFile(const std::string& filename)
{
    if (QemuAnswer::isEmulatorInstanceQEMU() == false)
    {
        return;
    }
    std::ifstream file(filename);
    if (file.is_open() == false)
    {
        return;
    }
    AnswerData* answer = nullptr;
    std::string line;
    while (std::getline(file, line))
    {
        boost::algorithm::trim(line);
        if (line.empty() == true || line.at(0) == '#') {continue;}
        if (line.at(0) == '[')
        {
            std::string function_name = createFunctionKey(line);
            std::unique_ptr<AnswerData> other(new AnswerData());
            _qemu_emulator_answers[function_name] = std::move(other);
            answer =  _qemu_emulator_answers[function_name].get();
        }
        else if (answer != nullptr)
        {
            answer->lines_data.push_back(line);
        }
    }
    file.close();
}
