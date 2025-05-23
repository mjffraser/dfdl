#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/socket.hpp"
#include "networking/messageFormatting.hpp"
#include "sourceInfo.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <ostream>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptServerCommunication
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to a server within a connection_timeout, send a
 *    request to that server, and receive a response from that server within
 *    response_timeout.
 *
 * Takes:
 * -> server:
 *    The server to connect to.
 * -> request:
 *    A server request formatted by messageFormatting
 * -> response_buff:
 *    A vector to read the response into.
 * -> connection_timeout:
 *    A timeout for how long to attempt connection for.
 * -> response_timeout:
 *    A timeout for how long to wait for the server to reply.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptServerCommunication(const  SourceInfo&           server,
                               const  std::vector<uint8_t>& request,
                                      std::vector<uint8_t>& response_buff,
                               const  uint8_t               msg_code,
                               struct timeval               connection_timeout,
                               struct timeval               response_timeout) {
    int sock = connectToSource(server, connection_timeout);
    if (sock < 0) return EXIT_FAILURE;

    int res = sendAndRecv(sock,
                          request,
                          response_buff,
                          msg_code,
                          response_timeout);

    closeSocket(sock);
    return res; //either EXIT_SUCCESS / EXIT_FAILURE
}

int attemptIndex(const  FileId&     file,
                 const  SourceInfo& server,
                 struct timeval     connection_timeout,
                 struct timeval     response_timeout) {
    std::vector<uint8_t> index_request = createIndexRequest(file);
    std::vector<uint8_t> server_response;
    if (index_request.empty()) return EXIT_FAILURE;

    return attemptServerCommunication(server,
                                      index_request,
                                      server_response,
                                      INDEX_OK,
                                      connection_timeout,
                                      response_timeout);
}

int attemptDrop(const  IndexUuidPair& file,
                const  SourceInfo&    server,
                struct timeval        connection_timeout,
                struct timeval        response_timeout) {
    std::vector<uint8_t> drop_request = createDropRequest(file);
    std::vector<uint8_t> server_response;
    if (drop_request.empty()) return EXIT_FAILURE;

    return attemptServerCommunication(server,
                                      drop_request,
                                      server_response,
                                      DROP_OK,
                                      connection_timeout,
                                      response_timeout);
}

int attemptControl(const  uint64_t file_uuid,
                const  SourceInfo&    faulty_client,
                const  SourceInfo&    server,
                struct timeval        connection_timeout,
                struct timeval        response_timeout){

    std::vector<uint8_t> control_request = createControlRequest(faulty_client, file_uuid);
    std::vector<uint8_t> server_response;
    if (control_request.empty()) return EXIT_FAILURE;

    return attemptServerCommunication(server,
                                      control_request,
                                      server_response,
                                      CONTROL_OK,
                                      connection_timeout,
                                      response_timeout);
}

int attemptSourceRetrieval(const uint64_t           file_uuid,
                           std::vector<SourceInfo>& dest,
                           const  SourceInfo&       server,
                           struct timeval           connection_timeout,
                           struct timeval           response_timeout) {
    dest.clear();
    std::vector<uint8_t> source_request = createSourceRequest(file_uuid);
    std::vector<uint8_t> server_response;
    if (source_request.empty()) return EXIT_FAILURE;

    if (EXIT_FAILURE == attemptServerCommunication(server,
                                                   source_request,
                                                   server_response,
                                                   SOURCE_LIST,
                                                   connection_timeout,
                                                   response_timeout)) {
        return EXIT_FAILURE;
    }

    auto servers = parseSourceList(server_response);
    for (auto& s : servers) dest.push_back(s);
    return EXIT_SUCCESS;
}

int attemptServerUpdate(std::vector<SourceInfo>& dest,
                        const  SourceInfo&       server,
                        struct timeval           connection_timeout,
                        struct timeval           response_timeout) {
    dest.clear();
    std::vector<uint8_t> server_response;

    if (EXIT_FAILURE == attemptServerCommunication(server,
                                                   {CLIENT_REG},
                                                   server_response,
                                                   REG_SERVERS_LIST,
                                                   connection_timeout,
                                                   response_timeout)) {
        return EXIT_FAILURE;
    }

    auto servers = parseServerRegResponse(server_response);
    for (const SourceInfo& s : servers) dest.push_back(s);
    return EXIT_SUCCESS;
}

} // namespace dfd
