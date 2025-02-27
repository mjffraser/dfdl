#include "networking/socket.hpp"
#include "sourceInfo.hpp"

#include <bits/types/struct_timeval.h>
#include <cstring>
#include <cstdint>
#include <optional>
#include <sys/socket.h>
#include <sys/types.h>
#include <utility>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace dfd {

struct SourceInfo;

std::optional<std::pair<int, uint16_t>> openSocket(){
    int socket_fd;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        return std::nullopt;
    }

    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));

    localAddr.sin_family        = AF_INET;
    localAddr.sin_port          = htons(0);     // use port 0 to select a free port
    localAddr.sin_addr.s_addr   = INADDR_ANY;   // OS assign ip 

    if (bind(socket_fd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        return std::nullopt;
    }

    socklen_t socket_len = sizeof(localAddr);
    if (getsockname(socket_fd, (struct sockaddr*)&localAddr, &socket_len) < 0) {
        return std::nullopt;
    }

    std::pair<int, uint16_t> p = std::make_pair(socket_fd, ntohs(localAddr.sin_port ));

    return p;
}


void closeSocket(int socket_fd){
    close(socket_fd);
}


int connect(int socket_fd, const SourceInfo& connect_to){
    struct sockaddr_in serverAddr;

    memset(&serverAddr, 0, sizeof(serverAddr));
    
    serverAddr.sin_family       = AF_INET;
    serverAddr.sin_port         = htons(connect_to.port);
    serverAddr.sin_addr.s_addr  = inet_addr(connect_to.ip_addr.c_str());
    
    if (connect(socket_fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        return -1;
    }

    return EXIT_SUCCESS;
}


int listen(int server_fd, int max_pending){
    if (listen(server_fd, max_pending) < 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


int accept(int server_fd, SourceInfo& client_info){
    struct sockaddr_in clientAddr;
    socklen_t client_len = sizeof(clientAddr);

    int client_fd = accept(server_fd, (struct sockaddr*)&clientAddr, &client_len);
    
    return client_fd;
}


int sendMessage(int socket_fd, const std::vector<uint8_t>& data) {
    size_t total_sent    = 0;
    size_t data_len      = data.size();

    while (total_sent < data_len) {
        ssize_t byte_sent = send(socket_fd, data.data() + total_sent, 
                                 data_len - total_sent, 0);

        if (byte_sent < 0) {
            return EXIT_FAILURE;
        }
        total_sent += byte_sent;
    }
    
    return EXIT_SUCCESS;
}


ssize_t recvData(int                   socket_fd, 
                 std::vector<uint8_t>& buffer, 
                 size_t                try_to_recv, 
                 timeval               timeout
                ){
    if (try_to_recv == 0) {
        return -1;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    int ret = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret <= 0) {
        return -1;
    }

    size_t original_len = buffer.size();
    buffer.resize(original_len + try_to_recv);

    ssize_t byte_read = recv(socket_fd, &buffer[original_len], try_to_recv, 0);

    if (byte_read > 0) {
        buffer.resize(original_len + byte_read);
        return byte_read;
    }

    buffer.resize(original_len);
    return -1;

}

}