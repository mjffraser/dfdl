#pragma once

#include <cstdint>
#include <string>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Config
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A struct to store basic config options offered by this software.
 *
 * Fields:
 * -> ip_addr:
 *    An IPV4 address to open a listening socket on.
 * -> bandwidth_limit:
 *    The max number of bytes to send every second.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
struct Config {
    std::string ip_addr         = "";
    uint64_t    bandwidth_limit = 0;
};


}
