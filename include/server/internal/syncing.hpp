#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>
#include <queue>

#include "server/internal/db.hpp"

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

                            //new adds
//called by new server to receive and merge database
int databaseReciveNS(int socket_fd, Database* db);

//sends database backup to the new server
int databaseSendNS(int socket_fd);

void massWriteSend(SourceInfo& new_server, std::queue<std::vector<uint8_t>> msg_queue);

} //dfd 
