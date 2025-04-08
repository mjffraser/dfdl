#include "client/internal/internal/seedThread.hpp"
#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"

namespace dfd {

void seedToPeer(std::atomic<bool> &shutdown, int peer_sock) {
    while (!shutdown.load()) {
        // recieve client download init request
        std::vector<uint8_t> buffer;
        if (!recvOkay(peer_sock, buffer, DOWNLOAD_INIT))
            return;

        auto [uuid, c_size] = parseDownloadInit(buffer);
        // TO-DO: rest of implementation
    }

    closeSocket(peer_sock); // Clean up the socket when done
}

} //dfd
