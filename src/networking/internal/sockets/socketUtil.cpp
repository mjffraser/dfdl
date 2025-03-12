#include "networking/internal/sockets/socketUtil.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"

#include <bits/types/struct_timeval.h>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace dfd{

void msgLenToBytes(const uint64_t len, uint8_t* buffer) {
    int err_code;
    uint64_t ordered = toNetworkOrder(len, err_code);
    std::memcpy(buffer, &ordered, sizeof(uint64_t)); 
}

uint64_t bytesToMsgLen(const std::vector<uint8_t>& buffer) {
    int err_code;
    uint64_t ordered;
    std::memcpy(&ordered, buffer.data(), sizeof(ordered));
    return fromNetworkOrder(ordered, err_code);
}

ssize_t recvBytes(int                  socket_fd, 
                 std::vector<uint8_t>& buffer, 
                 size_t                try_to_recv, 
                 timeval               timeout
                ) {
    if (try_to_recv == 0) {
        return -1;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    // int ret = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    int ret = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (ret < 0) {
        return -1;
    }

    size_t original_len = buffer.size();
    buffer.resize(original_len + try_to_recv);

    ssize_t bytes_read = recv(socket_fd, buffer.data()+original_len, try_to_recv, 0);

    if (bytes_read > 0) {
        buffer.resize(original_len + bytes_read);
        return bytes_read;
    }

    buffer.resize(original_len);
    return -1;
}

}
