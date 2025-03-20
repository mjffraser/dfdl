#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>

namespace dfd {

ssize_t forwardRegistration(std::vector<uint8_t>& reg_message,
                            const std::vector<SourceInfo>& servers);


std::vector<SourceInfo> forwardIndexRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);


std::vector<SourceInfo> forwardDropRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);


std::vector<SourceInfo> forwardReregRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);

void removeFailedServers(std::vector<SourceInfo>& known_servers,
                            const std::vector<SourceInfo>& failed_servers);

} //dfd 
