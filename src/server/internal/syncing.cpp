#include "server/internal/syncing.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"

namespace dfd {

ssize_t forwardRegistration(std::vector<uint8_t>& reg_message,
                            const std::vector<SourceInfo>& servers) {
    if (*reg_message.begin() != SERVER_REG)
        return -1;

    if (EXIT_SUCCESS != createForwardServerReg(reg_message))
        return -1;

    ssize_t registered_with = 0;

    for (auto& server : servers) {
        //open socket to talk to server
        auto sock = openSocket(false, 0);
        if (!sock)
            return registered_with;

        auto [server_sock, port] = sock.value();
        if (connect(server_sock, server) == -1) {
            closeSocket(server_sock);
            continue;
        }

        //we've connected, now send the forwarded server reg
        if (EXIT_SUCCESS != sendMessage(server_sock, reg_message)) {
            closeSocket(server_sock);
            continue;
        }

        //get response
        std::vector<uint8_t> server_response;
        timeval timeout;
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        if (recvMessage(server_sock, server_response, timeout) < 0) {
            closeSocket(server_sock);
            continue;
        }

        if (*server_response.begin() == FORWARD_SERVER_OK)
            registered_with++;
        closeSocket(server_sock);
    }

    return registered_with;
}

}
