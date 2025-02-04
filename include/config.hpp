#pragma once

#include <cstdint>
#include <string>

namespace dfd {

struct Config {
    std::string ip_addr         = "";
    uint16_t    port            = 0;
    uint64_t    bandwidth_limit = 0;
};


}
