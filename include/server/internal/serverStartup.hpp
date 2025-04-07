#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>
#include <queue>

#include "server/internal/db.hpp"

namespace dfd {


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

//NOTE: unwritten may change
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * joinNetwork
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Connects to another server at the address provided in known_server.
 *    Receives a database from that server to merge into open_db, as well as a
 *    list of all servers in the network, and appends these servers to
 *    known_servers. Initiates the transfer by sending a SERVER_REG message.
 *
 *    If this function fails at any point an error message is printed to stdout.
 *    No crash will occur.
 *
 * Takes:
 * -> known_server:
 *    The server to connect to.
 * -> open_db:
 *    The database owned by *this* server, that is merged into.
 * -> known_servers:
 *    The list of known_servers maintained by this server, which is modified by
 *    this registration process.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void joinNetwork(const SourceInfo&        known_server,
    Database*                open_db,
    std::vector<SourceInfo>& known_servers);

}//namespace