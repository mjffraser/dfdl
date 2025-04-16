#include <iostream>

#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/socket.hpp"

namespace dfd {

int connectToSource(const SourceInfo connect_to,
                    struct timeval connection_timeout) {
    auto sock = openSocket(false, 0); //server, port
    if (!sock) return -1;

    if (EXIT_SUCCESS != tcp::connect(sock->first, connect_to, connection_timeout)) {
        std::cout << "timed out" << std::endl;
        closeSocket(sock->first);
        return -1;
    }

    return sock->first;
}

bool sendOkay(int sock,
              const std::vector<uint8_t> &message) {
    if (tcp::sendMessage(sock, message) != EXIT_SUCCESS)
        return false;
    else
        return true;
}

bool recvOkay(int                           sock,
              std::vector<uint8_t>&         buffer,
              const uint8_t                 expected_code,
              std::optional<struct timeval> timeout) {
    buffer.clear();

    struct timeval recv_timeout;
    if (timeout) {
        recv_timeout.tv_sec  = timeout->tv_sec;
        recv_timeout.tv_usec = timeout->tv_usec;
    } else {
        recv_timeout.tv_sec  = RECV_TIMEOUT_SEC;
        recv_timeout.tv_usec = RECV_TIMEOUT_USEC;
    }

    ssize_t bytes_read = tcp::recvMessage(sock, buffer, recv_timeout);
    if (bytes_read < 0 || buffer.empty()) {
        return false;
    }

    return buffer[0] == expected_code;
}

int sendAndRecv(int                           sock_fd,
                const  std::vector<uint8_t>&  out,
                       std::vector<uint8_t>&  in,
                const  uint8_t                expected_code,
                std::optional<struct timeval> timeout) {
    if (!sendOkay(sock_fd, out))
        return EXIT_FAILURE;

    if (!recvOkay(sock_fd, in, expected_code, timeout))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

}
