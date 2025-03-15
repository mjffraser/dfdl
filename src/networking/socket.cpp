#include "networking/socket.hpp"
#include "networking/internal/sockets/socketUtil.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include "sourceInfo.hpp"

#include <bits/types/struct_timeval.h>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace dfd {

std::string getMyPublicIP() {
    char buff[16];
    FILE* res = popen("curl -s https://api64.ipify.org", "r");
    if (!res)
        return "";

    fgets(buff, 16, res);
    std::string ip;
    for (auto& c : buff)
        if (c)
            ip += c;

    pclose(res);
    return ip;
}

std::optional<std::pair<int, uint16_t>> openSocket(bool is_server, uint16_t port = 0) {
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return std::nullopt;
    }

    if (is_server) {
        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr)); // 0 out struct addr

        localAddr.sin_family        = AF_INET;
        localAddr.sin_port          = htons(port);
        localAddr.sin_addr.s_addr   = INADDR_ANY;   // OS assign ip 
    
        if (bind(socket_fd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            std::cout << "COULD NOT BIND" << std::endl;
            close(socket_fd);
            return std::nullopt;
        }
    
        socklen_t socket_len = sizeof(localAddr);
        if (getsockname(socket_fd, (struct sockaddr*)&localAddr, &socket_len) < 0) {
            close(socket_fd);
            return std::nullopt;
        }

        port = ntohs(localAddr.sin_port);
    }else {
        port = 0;
    }
    return std::make_pair(socket_fd, port);
}


void closeSocket(int socket_fd) {
    close(socket_fd);
}


int connect(int socket_fd, const SourceInfo& connect_to) {
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr)); // 0 out struct addr
    
    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_port         = htons(connect_to.port);
    serverAddr.sin_addr.s_addr  = inet_addr(connect_to.ip_addr.c_str());
    
    if (::connect(socket_fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(socket_fd);
        return -1;
    }

    return EXIT_SUCCESS;
}


int listen(int server_fd, int max_pending) {
    if (::listen(server_fd, max_pending) < 0) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


int accept(int server_fd, SourceInfo& client_info) {
    struct sockaddr_in clientAddr;
    socklen_t client_len = sizeof(clientAddr);
    memset(&clientAddr, 0, client_len); // 0 out struct addr

    int client_fd = ::accept(server_fd, (struct sockaddr*)&clientAddr, &client_len);

    if (client_fd < 0) {
        return -1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, INET_ADDRSTRLEN);
    client_info.ip_addr = client_ip;
    client_info.port = ntohs(clientAddr.sin_port);
    
    return client_fd;
}

int sendMessage(int socket_fd, const std::vector<uint8_t>& data) {
    uint64_t data_len = data.size();
    if (data_len == 0) {
        return EXIT_SUCCESS;
    }

    std::vector<uint8_t> data_msg;
    data_msg.resize(8+data.size());
    msgLenToBytes(data_len, data_msg.data());

    std::memcpy(data_msg.data()+8, data.data(), data.size());
    size_t sent = 0;
    while (sent < data_msg.size()) {
        ssize_t bytes_sent = send(socket_fd, data_msg.data()+sent, data_msg.size()-sent, 0);
        sent += bytes_sent;
    }

    return EXIT_SUCCESS;
}

ssize_t recvMessage(int                   socket_fd, 
                    std::vector<uint8_t>& buffer, 
                    timeval               timeout) {
    std::vector<uint8_t> header;
    ssize_t header_read = recvBytes(socket_fd, header, 8, timeout);
    if (header_read != 8) {
        std::cout << "Not reading 8 header bytes." << std::endl;
        return -1;
    }

    uint64_t data_len = bytesToMsgLen(header);

    size_t total_recv = 0;
    while (total_recv < data_len) {
        ssize_t bytes_read = recvBytes(socket_fd, buffer, data_len - total_recv, timeout);
        if (bytes_read < 0) {
            return -1;
        }
        total_recv += bytes_read;
    }

    buffer.resize(total_recv);
    return total_recv;
}

}
