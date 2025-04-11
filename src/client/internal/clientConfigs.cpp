#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <random>

#include "client/internal/clientConfigs.hpp"

namespace dfd {

int getHostListFromDisk(std::vector<SourceInfo> &dest, const std::string &host_path) {
    dest.clear();

    const std::filesystem::path p(std::filesystem::absolute(host_path));
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ifstream input_file(p);

    if (!input_file) {
        return EXIT_FAILURE;
    }

    auto trim = [](std::string& s) {
        const char* ws = " \t\r\n";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
    };

    std::string line;
    while (std::getline(input_file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        for (char& c : line) if (c == ',') c = ' ';

        std::istringstream iss(line);
        std::string peer_str, ip, port_str;
        if (!(iss >> peer_str >> ip >> port_str) || (iss >> std::ws, !iss.eof()))
            // wrong token count
            return EXIT_FAILURE;

        errno = 0;
        char* end = nullptr;
        unsigned long long peer_val = std::strtoull(peer_str.c_str(), &end, 10);
        if (end == peer_str.c_str() || *end != '\0' || errno == ERANGE)
            return EXIT_FAILURE;

        errno = 0;
        unsigned long port_val = std::strtoul(port_str.c_str(), &end, 10);
        if (end == port_str.c_str() || *end != '\0' || errno == ERANGE || port_val > 65535)
            return EXIT_FAILURE;

        dest.push_back(SourceInfo{
            static_cast<uint64_t>(peer_val),
            std::move(ip),
            static_cast<uint16_t>(port_val)
        });
    }

    return EXIT_SUCCESS;
}

int storeHostListToDisk(const std::vector<SourceInfo> &hosts, const std::string &host_path) {
    const std::filesystem::path p(host_path);
    if (!p.has_parent_path() || std::filesystem::create_directories(p.parent_path())) {
        // ok
    }

    std::ofstream out(host_path, std::ios::trunc);
    if (!out) {
        return EXIT_FAILURE;
    }

    for (const auto& h : hosts) {
        out << h.peer_id << ", " << h.ip_addr << ", " << h.port << '\n';
        if (!out)
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

uint64_t getMyUUID(const std::string &uuid_path) {
    uint64_t uuid = 0;

    // Try reading UUID from file
    if (std::ifstream uuid_file(uuid_path, std::ios::binary); uuid_file) {
        uuid_file.read(reinterpret_cast<char *>(&uuid), sizeof(uuid));
        if (uuid_file.gcount() == sizeof(uuid)) {
            std::cout << "Loaded UUID: " << uuid << " from file: " << uuid_path << std::endl;
            return uuid;
        }
        
        std::cout << "Error: UUID file exists but does not contain valid data.\n";
    }

    // Generate new UUID
    if (std::ifstream urandom("/dev/urandom", std::ios::binary); urandom) {
        urandom.read(reinterpret_cast<char *>(&uuid), 4); //TODO: THIS IS ONLY 4-BYTES, NOT 8
    }

    if (uuid == 0) { // fallback if /dev/urandom fails
        std::cout << "Warning: using random_device as fallback to generate uuid.\n";
        std::random_device rd;
        uuid = (static_cast<uint64_t>(rd()) << 32) | rd();
    }

    // Write new UUID to file
    if (std::ofstream uuid_file(uuid_path, std::ios::binary | std::ios::trunc); uuid_file) {
        uuid_file.write(reinterpret_cast<const char *>(&uuid), sizeof(uuid));
        std::cout << "Generated new UUID: " << uuid << " and saved to " << uuid_path << std::endl;
    } else {
        std::cout << "Error: Could not write new UUID to config file.\n";
    }

    return uuid;
}

} //dfd
