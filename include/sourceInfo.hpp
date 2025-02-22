#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/socket.h>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * SourceInfo
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A struct to store info about a client you're talking to.
 *
 * Fields:
 * -> ip_addr:
 *    Clients IPV4 address.
 * -> port:
 *    Clients port number.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
struct SourceInfo {
    uint64_t    peer_id;
    std::string ip_addr;
    uint16_t    port;
};

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * socketAddrToSourceInfo
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes information about an opened socket and extracts relevant info so the
 *    sockaddr struct can be safely destroyed.
 *
 * Takes:
 * -> info:
 *    A sockaddr struct populated by the accept() function in socket.hpp
 *
 * Returns:
 * -> On success:
 *    SourceInfo struct.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<SourceInfo> socketAddrToSourceInfo(sockaddr*& info);

}
