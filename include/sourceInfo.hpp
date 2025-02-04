#pragma once

#include <cstdint>
#include <string>

namespace dfd {

struct SourceInfo {
    std::string ip_addr;
    uint16_t    port;
};

}
