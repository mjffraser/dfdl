#include "client/internal/internal/internal/downloadHandshake.hpp"
#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include <iostream>
#include <ostream>
#include <vector>

namespace dfd {

int attemptDownloadHandshake(int            connected_sock,
                             const uint64_t f_uuid,
                             std::string&   f_name,
                             uint64_t&      f_size,
                             struct timeval response_timeout) {
    std::vector<uint8_t> download_init = createDownloadInit(f_uuid, std::nullopt);
    if (download_init.empty())
        return EXIT_FAILURE;

    std::vector<uint8_t> peer_response;
    int res = sendAndRecv(connected_sock,
                          download_init,
                          peer_response,
                          DOWNLOAD_CONFIRM,
                          response_timeout);
    if (res == EXIT_FAILURE) {
        closeSocket(connected_sock);
        return EXIT_FAILURE;
    }
    
    auto [size, name] = parseDownloadConfirm(peer_response);
    if (size == 0) {
        closeSocket(connected_sock);
        return EXIT_FAILURE;
    }

    f_size = size;
    f_name = name;
    return EXIT_SUCCESS;
}

} //dfd
