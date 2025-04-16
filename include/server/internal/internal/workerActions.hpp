#pragma once

#include "sourceInfo.hpp"
#include "server/internal/db.hpp"

#include <cstdint>
#include <vector>

namespace dfd {


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientIndexRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a client's INDEX_REQUEST, inserts the entry into the database, and
 *    returns a message to be sent back to the client, regardless of success or
 *    failure.
 *
 * Takes:
 * -> client_request:
 *    The message received from the client that begins with either INDEX_REQUEST
 *    or INDEX_FORWARD.
 * -> response_dest:
 *    The vector to write the response message that can be sent back to the
 *    requesting client.
 * -> db:
 *    The database to modify.
 *
 * Note: This function will write an error message to the response_dest on
 *       error, so no return code is used.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientIndexRequest(const std::vector<uint8_t>& client_request,
                             std::vector<uint8_t>&  response_dest,
                             Database*              db);
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientDropRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a client's DROP_REQUEST, removes the entry from the database, and
 *    returns a message to be sent back to the client, regardless of success or
 *    failure.
 *
 * Takes:
 * -> client_request:
 *    The message received from the client that begins with either DROP_REQUEST
 *    or DROP_FORWARD.
 * -> response_dest:
 *    The vector to write the response message that can be sent back to the
 *    requesting client.
 * -> db:
 *    The database to modify.
 *
 * Note: This function will write an error message to the response_dest on
 *       error, so no return code is used.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientDropRequest(const std::vector<uint8_t>& client_request,
                            std::vector<uint8_t>&  response_dest,
                            Database*              db);
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientReregisterRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a client's REREGISTER_REQUEST, updates the relevant row in the
 *    database, and returns a message to be sent back to the client, regardless
 *    of success or failure.
 *
 * Takes:
 * -> client_request:
 *    The message received from the client that begins with either
 *    REREGISTER_REQUEST or REREGISTER_FORWARD.
 * -> response_dest:
 *    The vector to write the response message that can be sent back to the
 *    requesting client.
 * -> db:
 *    The database to modify.
 *
 * Note: This function will write an error message to the response_dest on
 *       error, so no return code is used.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientReregisterRequest(const std::vector<uint8_t>& client_request,
                                  std::vector<uint8_t>&  response_dest,
                                  Database*              db);
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientSourceRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a client's SOURCE_REQUEST, performs a select on the database to get
 *    indexing peers for their requested file, and returns a message to be sent
 *    back to the client, regardless of success or failure.
 *
 * Takes:
 * -> client_request:
 *    The message received from the client that begins with SOURCE_REQUEST.
 * -> response_dest:
 *    The vector to write the response message that can be sent back to the
 *    requesting client.
 * -> db:
 *    The database to modify.
 *
 * Note: This function will write an error message to the response_dest on
 *       error, so no return code is used.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientSourceRequest(const std::vector<uint8_t>& client_request,
                              std::vector<uint8_t>&  response_dest,
                              Database*              db);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientServerRegistration
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Adds a new server to known servers than forwards to all other servers a
 *    forwarded server reg. Than makes responce = known_servers.
 *
 * Takes:
 * -> client_request:
 *    The message received from the client that begins with SERVER_REG.
 * -> response_dest:
 *    The vector to write the response message that can be sent back to the
 *    requesting client.
 * -> known_servers:
 *    A pointer to the known servers vector to modify.
 * -> db:
 *    A database pointer
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void serverToServerRegistration(std::vector<uint8_t>&      client_request,
                                std::vector<uint8_t>&    response_dest,
                                std::vector<SourceInfo>& known_servers,
                                std::mutex&              knowns_mtx,
                                Database*                       db);

} //dfd
