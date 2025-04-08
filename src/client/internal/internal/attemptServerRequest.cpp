#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/socket.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/fileParsing.hpp"
#include "networking/internal/fileParsing/fileUtil.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include "sourceInfo.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <filesystem>

namespace dfd {

bool isFail(const std::vector<uint8_t>& msg) {
    return !msg.empty() && msg[0] == FAIL;
}

bool isOk (const std::vector<uint8_t>& msg, uint8_t expected) {
    return !msg.empty() && msg[0] == expected;
}

int attemptIndex(const  FileId&     file,
                 const  SourceInfo& server,
                 struct timeval     connection_timeout,
                 struct timeval     response_timeout) {
    std::vector<uint8_t> index_request = createIndexRequest(file);
    if (index_request.empty()) return EXIT_FAILURE;

    int sock = connectToSource(server, connection_timeout);
    if (sock < 0) return EXIT_FAILURE;

    std::vector<uint8_t> server_response;
    int res = sendAndRecv(sock,
                          index_request,
                          server_response,
                          INDEX_OK,
                          response_timeout);
    closeSocket(sock);
    return res; //either EXIT_SUCCESS / EXIT_FAILURE
}

int attemptDrop(const  IndexUuidPair& file,
                const  SourceInfo&    server,
                struct timeval        connection_timeout,
                struct timeval        response_timeout) {
    std::vector<uint8_t> drop_request = createDropRequest(file);
    if (drop_request.empty()) return EXIT_FAILURE;

    int sock = connectToSource(server, connection_timeout);
    if (sock < 0) return EXIT_FAILURE;

    std::vector<uint8_t> server_response;
    int res = sendAndRecv(sock,
                          drop_request,
                          server_response,
                          DROP_OK,
                          response_timeout);
    std::cout << res << " " << EXIT_SUCCESS << std::endl;
    closeSocket(sock);
    return res; //either EXIT_SUCCESS / EXIT_FAILURE
}


// int attemptSourceRetrieval(const uint64_t file_uuid,
//                            std::vector<SourceInfo> &dest)
// {
//     // dest.clear();
//     //
//     // std::filesystem::path hosts_file = configDir() / HOSTS_FILE_NAME;
//     // std::vector<SourceInfo> hosts;
//     // if (getHostListFromDisk(hosts, hosts_file.string()) != EXIT_SUCCESS
//     //     || hosts.empty())
//     //     return EXIT_FAILURE;
//     //
//     // std::vector<uint8_t> req = createSourceRequest(file_uuid);
//     // if (req.empty()) return EXIT_FAILURE;
//     //
//     // for (auto& h : hosts)
//     // {
//     //     auto sock = openSocket(false, 0, false);
//     //     if (!sock) continue;
//     //     struct timeval timeout{3,0};
//     //     if (tcp::connect(sock->first, h, timeout) != EXIT_SUCCESS)
//     //     { closeSocket(sock->first); continue; }
//     //
//     //     std::vector<uint8_t> resp;
//     //     if (sendAndRecv(sock->first, req, resp, timeout) != EXIT_SUCCESS)
//     //     { closeSocket(sock->first); continue; }
//     //
//     //     closeSocket(sock->first);
//     //
//     //     if (isFail(resp))
//     //         continue;
//     //     if (isOk(resp, SOURCE_LIST))
//     //     {
//     //         dest = parseSourceList(resp);
//     //         return EXIT_SUCCESS;
//     //     }
//     // }
//     return EXIT_FAILURE;
// }

} // namespace dfd
