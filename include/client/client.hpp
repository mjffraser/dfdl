#pragma once

#include <cstdint>
#include <string>

namespace dfd {

void run_client(const std::string& ip,
               uint16_t           port,
               const std::string& download_dir,
               const std::string& listen_addr);

}

