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

#include <sys/types.h>
#include <sys/inotify.h>
#include <systemd/sd-event.h>
#include <string>
#include <functional>

typedef std::function<void()> FUNCTION_HANDLER;

namespace phosphor
{
namespace software
{
namespace updater
{

class ImageWatcher
{
 public:
      ImageWatcher();
      virtual ~ImageWatcher();
      int createWatcher(const std::string &versionId);
      static int onImageRemoved(sd_event_source*  /*source*/,
                       int fd,
                       uint32_t revents,
                       void* userdata);
      void watch(FUNCTION_HANDLER funcionCallBack);
      void clear();
 private:
     int m_notifyFd  = -1;
     int m_watchFd   = -1;
     int m_done      = 0;
     sd_event* m_loop = nullptr;
};

} // namespace updater
} // namespace software
} // namespace phosphor
