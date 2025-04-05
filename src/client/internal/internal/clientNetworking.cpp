#include <iostream>

#include "networking/socket.hpp"
#include "client/internal/internal/clientNetworking.hpp"

namespace dfd
{
    int connectToSource(const SourceInfo connect_to,
                        struct timeval connection_timeout)
    {
        auto sock_info = openSocket(false, 0);
        if (!sock_info.has_value())
        {
            std::cerr << "Failed to open client socket.\n";
            return -1;
        }

        auto [client_socket_fd, ephemeral_port] = sock_info.value();
        std::cout << "Client socket_fd = " << client_socket_fd
                  << ", ephemeral port = " << ephemeral_port << std::endl;

        if (tcp::connect(client_socket_fd, connect_to, connection_timeout) == -1)
        {
            std::cerr << "Failed to connect to source ("
                      << connect_to.ip_addr << ":" << connect_to.port << ").\n";
            closeSocket(client_socket_fd);
            return -1;
        }

        std::cout << "Successfully connected to source at "
                  << connect_to.ip_addr << ":" << connect_to.port << "\n";

        return client_socket_fd;
    }

    bool sendOkay(int sock,
                  const std::vector<uint8_t> &message)
    {
        if (tcp::sendMessage(sock, message) != EXIT_SUCCESS)
            return false;
        else
            return true;
    }

    bool recvOkay(int sock,
                  std::vector<uint8_t> &buffer,
                  const uint8_t expected_code)
    {
        buffer.clear();

        struct timeval tv;
        tv.tv_sec = RECV_TIMEOUT_SEC;
        tv.tv_usec = RECV_TIMEOUT_USEC;

        ssize_t bytes_read = tcp::recvMessage(sock, buffer, tv);
        if (bytes_read < 0 || buffer.empty())
        {
            return false;
        }

        return buffer[0] == expected_code;
    }
}