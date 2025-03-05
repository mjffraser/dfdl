#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <iostream>
#include <ostream>
#include <string>
#include <sys/socket.h>

namespace dfd {

uint32_t getIpBytes(const std::string& ip_str) {
    uint32_t ip_bytes;
    if (0 >= inet_pton(AF_INET, ip_str.c_str(), &ip_bytes))
        return 0;
    return ip_bytes;
}

std::string ipBytesToString(const uint8_t* ip_bytes) {
    char buff[INET_ADDRSTRLEN];
    uint32_t network_order_bytes;
    std::memcpy(&network_order_bytes, ip_bytes, sizeof(network_order_bytes));
    if (nullptr == inet_ntop(AF_INET, &network_order_bytes, buff, INET_ADDRSTRLEN))
        return "";
    return std::string(buff);
}

}
