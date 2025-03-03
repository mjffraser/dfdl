#include "networking/internal/sockets/socketUtil.hpp"

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

void sizetToBytes(size_t val, uint8_t* buffer) {
    for (size_t i = 0; i < 8; ++i) {
        buffer[i] = (val >> (i*8)) & 0xFF;
    }
}

size_t bytesToSizet(std::vector<uint8_t>& buffer) {
    size_t val;
    for (size_t i = 0; i < 8; ++i) {
        val = (val << 8) | buffer[7-i];
    }
    return val;
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
    int ret = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret <= 0) {
        return -1;
    }

    size_t original_len = buffer.size();
    buffer.resize(original_len + try_to_recv);

    ssize_t bytes_read = recv(socket_fd, &buffer[original_len], try_to_recv, 0);

    if (bytes_read > 0) {
        buffer.resize(original_len + bytes_read);
        return bytes_read;
    }

    buffer.resize(original_len);
    return -1;
}

}
