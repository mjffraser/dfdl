#pragma once

#include <cstdint>
#include <string>
//#include <sys/socket.h>
#include <winsock2.h> //ONLY FOR WINDOWS

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

}
