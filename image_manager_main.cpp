#include "config.h"

#include "image_manager.hpp"
#include "watch.hpp"

#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

std::vector<std::string> getSoftwareObjects(sdbusplus::bus::bus& bus)
{
    std::vector<std::string> paths;
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_INTERFACE, "GetSubTreePaths");
    method.append(SOFTWARE_OBJPATH);
    method.append(0); // Depth 0 to search all
    method.append(std::vector<std::string>({VERSION_BUSNAME}));
    auto reply = bus.call(method);
    reply.read(paths);
    return paths;
}


int main()
{
    using namespace phosphor::software::manager;
    auto bus = sdbusplus::bus::new_default();

    sd_event* loop = nullptr;
    sd_event_default(&loop);

    sdbusplus::server::manager::manager objManager(bus, SOFTWARE_OBJPATH);
    bus.request_name(VERSION_BUSNAME);

    try
    {
        phosphor::software::manager::Manager imageManager(bus);
        phosphor::software::manager::Watch watch(
            loop, std::bind(std::mem_fn(&Manager::processImage), &imageManager,
                            std::placeholders::_1));
        bus.attach_event(loop, SD_EVENT_PRIORITY_NORMAL);
        sd_event_loop(loop);
    }
    catch (std::exception& e)
    {
        using namespace phosphor::logging;
        log<level::ERR>(e.what());
        return -1;
    }

    sd_event_unref(loop);
	std::string id = "28bd62d9";
	for (int host = 1; host <= 4; host++)
    {
        auto objPath = std::string{SOFTWARE_OBJPATH} + "/host" +
                       std::to_string(host) + "/" + id; 
        auto allSoftwareObjs = getSoftwareObjects(bus);
        auto it =
            std::find(allSoftwareObjs.begin(), allSoftwareObjs.end(), objPath);
        if (it == allSoftwareObjs.end())
        {
            std::cerr<<"In  main Object not preset here for host" << host <<"\n";
        }
        else
        {
            std::cerr<<"In main Object availabel for host" << host <<" "<<*it <<"\n";
        }   
    }   

    return 0;
}
