#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>
#include <queue>

#include "server/internal/db.hpp"

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * forwardRegistration
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Forwards a SERVER_REG message to all known servers to notify them of a
 *    new server joining the network, waits for there acks
 *
 *    will retry once(can change) per server if connection or transmission fails
 *
 * Takes:
 * -> reg_message:
 *    the SERVER_REG message to forward, will be converted to FORWARD_SERVER_REG.
 * -> servers:
 *    the list of known servers
 *
 * Returns:
 * -> number of servers that acknowledged the forwarded registration.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
ssize_t forwardRegistration(std::vector<uint8_t>& reg_message,
                            const std::vector<SourceInfo>& servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * forwardIndexRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> wrapper around forwardRequest() for INDEX_REQUEST messages
 *
 * Takes:
 * -> initial_msg:
 *    the INDEX_REQUEST message to forward
 * -> servers:
 *    the list of target servers
 *
 * Returns:
 * -> list of servers that failed the forwarded index
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<SourceInfo> forwardIndexRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * forwardDropRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> wrapper around forwardRequest() for DROP_REQUEST messages
 *
 * Takes:
 * -> initial_msg:
 *    the DROP_REQUEST message to forward
 * -> servers:
 *    the list of target servers
 *
 * Returns:
 * -> list of servers that failed the forwarded index
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<SourceInfo> forwardDropRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * forwardReregRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> wrapper around forwardRequest() for REREGISTER_REQUEST messages.
 *
 * Takes:
 * -> initial_msg:
 *    the REREGISTER_REQUEST message to forward
 * -> servers:
 *    the list of target servers
 *
 * Returns:
 * -> list of servers that failed the forwarded reregister
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<SourceInfo> forwardReregRequest(
                            std::vector<uint8_t>& initial_msg,
                            const std::vector<SourceInfo>& servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * removeFailedServers
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> removes any server from the known_servers list that is present in the
 *    failed_servers list (based on IP and port match)
 *
 * Takes:
 * -> known_servers:
 *    the local list of currently known servers (will be modified)
 * -> failed_servers:
 *    the list of servers to remove from known_servers
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void removeFailedServers(std::vector<SourceInfo>& known_servers,
                            const std::vector<SourceInfo>& failed_servers,
                            std::mutex&                     known_server_mtx);


} //dfd 
