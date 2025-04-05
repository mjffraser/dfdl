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
                            const std::vector<SourceInfo>& failed_servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * databaseReciveNS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> called by a new server to receive and merge a backup of the network
 *    database from another server.
 *
 * Takes:
 * -> socket_fd:
 *    the connected socket to the sending server
 * -> db:
 *    a pointer to the database
 *
 * Returns:
 * -> EXIT_SUCCESS on success, EXIT_FAILURE on eror
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int databaseReciveNS(int socket_fd, Database* db);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * databaseSendNS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> sends a full backup of the current server database to a new server
 *    joining the network, using file chunk send API
 *
 * Takes:
 * -> socket_fd:
 *    the connected socket to the receiving server
 *
 * Returns:
 * -> EXIT_SUCCESS on success, EXIT_FAILURE on eror
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int databaseSendNS(int socket_fd);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * massWriteSend
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> sends a series of queued write messages (INDEX, DROP, REREGISTER and
 *    their FORWARD variants) to a newly registered server. Converts messages
 *    to forward variants if they are not a forward message.
 *
 * Takes:
 * -> new_server:
 *    The destination server to send the queued messages to
 * -> msg_queue:
 *    queue of raw messages to be sent
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void massWriteSend(SourceInfo& new_server, std::queue<std::vector<uint8_t>> msg_queue);

} //dfd 
