#pragma once

#include <cstdint>
#include <vector>

namespace dfd {

std::vector<uint8_t> left_encode(std::vector<uint8_t>& message);

std::vector<uint8_t> strip_left_encode(std::vector<uint8_t>& encoded_message);

} //dfd
