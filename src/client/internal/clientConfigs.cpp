#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>

#include "client/internal/clientConfigs.hpp"
namespace dfd
{
    int getHostListFromDisk(std::vector<SourceInfo> &dest, const std::string &host_path)
    {
    }

    int storeHostListToDisk(const std::vector<SourceInfo> &hosts, const std::string &host_path)
    {
    }

    uint64_t getMyUUID(const std::string &uuid_path)
    {
        std::filesystem::create_directory(uuid_path);
        uint64_t uuid = 0;

        // Try reading UUID from file
        if (std::ifstream uuid_file(uuid_path, std::ios::binary); uuid_file)
        {
            uuid_file.read(reinterpret_cast<char *>(&uuid), sizeof(uuid));
            if (uuid_file.gcount() == sizeof(uuid))
            {
                std::cout << "Loaded UUID: " << uuid << "from " << uuid_path << std::endl;
                return uuid;
            }
            std::cout << "Error: UUID file exists but does not contain valid data.\n";
        }

        // Generate new UUID
        if (std::ifstream urandom("/dev/urandom", std::ios::binary); urandom)
        {
            urandom.read(reinterpret_cast<char *>(&uuid), sizeof(uuid));
        }
        if (uuid == 0)
        { // Fallback if /dev/urandom fails
            std::cout << "Warning: Using random_device as fallback to generate UUID.\n";
            std::random_device rd;
            uuid = (static_cast<uint64_t>(rd()) << 64) | rd();
        }

        // Write new UUID to file
        if (std::ofstream uuid_file(uuid_path, std::ios::binary | std::ios::trunc); uuid_file)
        {
            uuid_file.write(reinterpret_cast<const char *>(&uuid), sizeof(uuid));
            std::cout << "Generated new UUID: " << uuid << "and saved to " << uuid_path << std::endl;
        }
        else
        {
            std::cout << "Error: Could not write new UUID to config file.\n";
        }

        return uuid;
    }
}