#include "client/internal/requests.hpp"
#include "client/internal/internal/attemptServerRequest.hpp"
#include "client/internal/internal/attemptPeerRequest.hpp"
#include "networking/fileParsing.hpp"
#include "networking/messageFormatting.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>

namespace dfd {

static bool           timeout_init = false;
static struct timeval connection_timeout;
static struct timeval response_timeout;

void init_timeouts() {
    //CONNECTION TIMEOUT: 0.5s
    connection_timeout.tv_sec  = 0;
    connection_timeout.tv_usec = 500000;

    //RESPONSE TIMEOUT: 2s
    response_timeout.tv_sec  = 2;
    response_timeout.tv_usec = 0;

    timeout_init = true;
}

int updateServerList(std::vector<SourceInfo>& server_list) {
    //TODO w/ server sync
    return EXIT_SUCCESS;
}

std::optional<FileId> parseFile(const SourceInfo&  my_listener,
                                const std::string& f_path) {
    if (!std::filesystem::exists(f_path)) {
        std::cerr << "[err] file cannot be found." << std::endl;
        return std::nullopt;
    }

    uint64_t f_uuid = sha256Hash(f_path);
    if (f_uuid == 0) {
        std::cerr << "[err] file uuid could not be computed." << std::endl;
        return std::nullopt;
    }

    auto f_size_opt = fileSize(f_path);
    if (!f_size_opt) {
        std::cerr << "[err] failed to get file size." << std::endl;
        return std::nullopt;
    }

    return FileId(f_uuid, my_listener, f_size_opt.value());
}

int doIndex(const SourceInfo&               my_listener,
            const std::string&              file_path,
                  std::vector<std::string>& indexed_files,
                  std::vector<SourceInfo>&  server_list) {
    if (!timeout_init) init_timeouts();

    auto file = parseFile(my_listener, file_path); 
    if (!file)
        return EXIT_FAILURE;
    FileId& f_info = file.value();

    std::vector<SourceInfo> bad_servers;
    std::cout << "Indexing..." << std::endl;
    bool indexed = false;
    for (int i = 0; i < 2 && !indexed; ++i) {
        if (i == 1) {}//TODO UPDATE SERVER LIST WITH SYNC
        for (const SourceInfo& server : server_list) {
            if (EXIT_SUCCESS == attemptIndex(f_info,
                                             server,
                                             connection_timeout,
                                             response_timeout)) {
                indexed = true;
                break;
            } else {
                if (i == 1) 
                    bad_servers.push_back(server); //record on second attempt
            }
        }
    }

    if (server_list.size() == bad_servers.size()) {
        std::cerr << "[err] Sorry, tried " << bad_servers.size() << " server(s) twice, and received no response." << std::endl;
        return EXIT_FAILURE;
    } else {
        indexed_files.push_back(std::filesystem::absolute(file_path));
        std::cout << "File: '" << f_info.uuid << "' is now indexed with the DFD network." << std::endl;
        return EXIT_SUCCESS;
    }
}

int doDrop(const SourceInfo&               my_listener,
           const std::string&              file_path,
                 std::vector<std::string>& indexed_files,
                 std::vector<SourceInfo>&  server_list) {
    if (!timeout_init) init_timeouts();
    auto file_it = std::find(indexed_files.begin(),
                             indexed_files.end(),
                             std::filesystem::absolute(file_path));
    if (file_it == indexed_files.end()) {
        std::cerr << "This file is not currently indexed." << std::endl;
        return EXIT_FAILURE;
    }

    auto file = parseFile(my_listener, file_path); 
    if (!file)
        return EXIT_FAILURE;
    FileId& f_info = file.value();
    IndexUuidPair drop_pair = std::make_pair(f_info.uuid, f_info.indexer.peer_id);

    std::vector<SourceInfo> bad_servers;
    std::cout << "Dropping..." << std::endl;
    bool dropped = false;
    for (int i = 0; i < 2 && !dropped; ++i) {
        if (i == 1) {}//TODO UPDATE SERVER LIST WITH SYNC
        for (const SourceInfo& server : server_list) {
            if (EXIT_SUCCESS == attemptDrop(drop_pair,
                                            server,
                                            connection_timeout,
                                            response_timeout)) {
                dropped = true;
                break;
            } else {
                if (i == 1)
                    bad_servers.push_back(server); //record on second attempt
            }
        }
    }

    if (server_list.size() == bad_servers.size()) {
        std::cerr << "[err] Sorry, tried " << bad_servers.size() << " server(s) twice, and received no response." << std::endl;
        return EXIT_FAILURE;
    } else {
        indexed_files.erase(file_it);
        std::cout << "File: '" << f_info.uuid << "' is now dropped from the DFD network." << std::endl;
        return EXIT_SUCCESS;
    }
}

} //dfd

