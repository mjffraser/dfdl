#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>

namespace dfd {

ssize_t forwardRegistration(std::vector<uint8_t>& reg_message,
                            const std::vector<SourceInfo>& servers);

} //dfd 
