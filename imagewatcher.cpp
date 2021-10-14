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

#include "imagewatcher.hpp"
#include <unistd.h>
#include <filesystem>
#include <string>

#ifndef IMG_UPLOAD_DIR
#define IMG_UPLOAD_DIR  "/tmp/images"
#endif

namespace
{
  uint32_t  m_rmMask =  IN_DELETE    | IN_DELETE_SELF | IN_MODIFY
                      | IN_MOVE_SELF | IN_MOVED_FROM  | IN_MOVED_TO;
}

namespace phosphor
{
namespace software
{
namespace updater
{

ImageWatcher::ImageWatcher()
{
    // Empty
}


ImageWatcher::~ImageWatcher()
{
    clear();
}


int ImageWatcher::onImageRemoved(sd_event_source*  /*source*/,
                                 int fd,
                                 uint32_t revents,
                                 void* userdata)
{
    if ((revents & EPOLLIN) == 0)
    {
        return 0;
    }
    constexpr int maxBytes = 1024;
    uint8_t buffer[maxBytes];
    auto bytes = ::read(fd, buffer, maxBytes);
    auto offset = 0;
    while (offset < bytes)
    {
        auto event = reinterpret_cast<inotify_event*>(&buffer[offset]);
        if (event->mask & m_rmMask)
        {
            int *done = static_cast<int*>(userdata);
            *done = 1;
            return 0;  // OK, that is expected
        }
        offset += offsetof(inotify_event, name) + event->len;
    }
    return -1;
}

void ImageWatcher::watch(FUNCTION_HANDLER funcionCallBack)
{
    uint64_t half_second = 500000;
    while (m_done == 0)
    {
        ::sleep(2);
        ::sd_event_run(m_loop, half_second);
    }
    if (m_done != 0 && funcionCallBack != nullptr)
    {
          funcionCallBack();
    }
    clear();
}


void ImageWatcher::clear()
{
    if (m_notifyFd > 0 && m_watchFd > 0)
    {
        ::inotify_rm_watch(m_notifyFd, m_watchFd);
    }
    if (m_notifyFd > 0)
    {
        ::close(m_notifyFd);
    }
    m_notifyFd = -1;
    m_watchFd  = -1;
    m_loop     = nullptr;
    m_done     = 0;
}


int ImageWatcher::createWatcher(const std::string& versionId)
{
    ::sd_event_default(&m_loop);
    m_notifyFd = ::inotify_init1(IN_NONBLOCK);
    std::string image(IMG_UPLOAD_DIR);
    std::filesystem::path image_dir(image  + '/' + versionId);
    m_watchFd  = ::inotify_add_watch(m_notifyFd, image_dir.c_str(), m_rmMask);
    if (m_notifyFd > 0 && m_watchFd > 0)
    {
        auto rc = sd_event_add_io(m_loop, nullptr, m_notifyFd, EPOLLIN,
                                  &ImageWatcher::onImageRemoved, &m_done);
        if (rc >= 0)
        {
            return 0;
        }
    }
    return -1;
}

} // namespace updater
} // namespace software
} // namespace phosphor
