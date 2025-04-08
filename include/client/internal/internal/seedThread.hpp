#pragma once

#include <atomic>
namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * seedToPeer
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A function to handle a client request for file chunks. This function is
 *    designed to be opened as a thread that can be flagged for shutdown, and
 *    is not detatched.
 *
 * Takes:
 * -> shutdown:
 *    A atomic bool that, if set True, this function will make its best effort
 *    to exit as fast as possible to by joined.
 * -> peer_sock:
 *    The socket connection to the peer requesting a file download.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void seedToPeer(std::atomic<bool>& shutdown, int peer_sock);

} //dfd
