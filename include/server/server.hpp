#pragma once

#include <cstdint>
#include <string>

namespace dfd {

int run_server(const uint16_t     port, 
               const std::string& connect_ip, 
               const uint16_t     connect_port);

} //namespace dfd
