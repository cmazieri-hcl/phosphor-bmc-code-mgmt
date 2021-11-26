#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <boost/algorithm/string.hpp>
#include <unistd.h>

namespace utils
{

PHOSPHOR_LOG2_USING;

std::string getService(sdbusplus::bus::bus& bus, const std::string& path,
                       const std::string& interface)
{
    auto method = bus.new_method_call(MAPPER_BUSNAME, MAPPER_PATH,
                                      MAPPER_BUSNAME, "GetObject");

    method.append(path);
    method.append(std::vector<std::string>({interface}));

    std::vector<std::pair<std::string, std::vector<std::string>>> response;

    try
    {
        auto reply = bus.call(method);
        reply.read(response);
        if (response.empty())
        {
            error(
                "Empty response from mapper for getting service name: {PATH} {INTERFACE}",
                "PATH", path, "INTERFACE", interface);
            return std::string{};
        }
    }
    catch (const sdbusplus::exception::exception& e)
    {
        error("Error in mapper method call for ({PATH}, {INTERFACE}: {ERROR}",
              "ERROR", e, "PATH", path, "INTERFACE", interface);
        return std::string{};
    }
    return response[0].first;
}

void mergeFiles(std::vector<std::string>& srcFiles, std::string& dstFile)
{
    std::ofstream outFile(dstFile, std::ios::out);
    for (auto& file : srcFiles)
    {
        std::ifstream inFile;
        inFile.open(file, std::ios_base::in);
        if (!inFile)
        {
            continue;
        }

        inFile.peek();
        if (inFile.eof())
        {
            inFile.close();
            continue;
        }

        outFile << inFile.rdbuf();
        inFile.close();
    }
    outFile.close();
}

namespace internal
{

/* @brief Helper function to build a string from command arguments */
static std::string buildCommandStr(const char* name, char** args)
{
    std::string command = name;
    for (int i = 0; args[i]; i++)
    {
        command += " ";
        command += args[i];
    }
    return command;
}

int executeCmd(const char* path, char** args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execv(path, args);

        // execv only retruns on err
        auto err = errno;
        auto command = buildCommandStr(path, args);
        error("Failed ({ERRNO}) to execute command: {COMMAND}", "ERRNO", err,
              "COMMAND", command);
        return -1;
    }
    else if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) < 0)
        {
            error("Error ({ERRNO}) during waitpid.", "ERRNO", errno);
            return -1;
        }
        else if (WEXITSTATUS(status) != 0)
        {
            auto command = buildCommandStr(path, args);
            error("Error ({STATUS}) occurred when executing command: {COMMAND}",
                  "STATUS", status, "COMMAND", command);
            return -1;
        }
    }
    else
    {
        error("Error ({ERRNO}) during fork.", "ERRNO", errno);
        return -1;
    }

    return 0;
}

}

bool isMultiHostMachine()
{
    bool multihost = false;  // single host is the default
#if defined(OBMC_HOST_INSTANCES)
    // multi hosts list can be separated by spaces or commas
    if (::strcmp("0", OBMC_HOST_INSTANCES) != 0
            && (   ::strchr(OBMC_HOST_INSTANCES, ' ') != nullptr
                || ::strchr(OBMC_HOST_INSTANCES, ',') != nullptr))
    {
        multihost = true;
    }
#endif
    return multihost;
}

std::vector<std::string> getMultiHostIds()
{
    std::vector<std::string>  ids;
#if defined(OBMC_HOST_INSTANCES)
    if (isMultiHostMachine() == true)
    {
        std::string host_instances{OBMC_HOST_INSTANCES};
        char delimiter[] = ",";
        if  (host_instances.find(delimiter[0]) == std::string::npos)
        {
            delimiter[0] = ' ';
        }
        boost::algorithm::split(ids, host_instances,
                                 boost::is_any_of(delimiter));
        for (auto& id : ids)
        {
                boost::algorithm::trim(id);  // remove any extra space
        }
    }
#endif
    return ids;
}



// namespace internal

} // namespace utils
