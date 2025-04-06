#pragma once

#include <atomic>

namespace dfd {

#define MAX_PEER_THREADS 5

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

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientListener
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens a socket to listen for incoming connections. Records the port it
 *    opened on in the provided atomic uint16_t. This function is designed to be
 *    opened as a thread that can be flagged for shutdown, and is not detatched.
 *    This function will join all seedThreads it has open before returning
 *    itself.
 *
 * Takes:
 * -> shutdown:
 *    A atomic bool that, if set True, this function will make its best effort
 *    to exit as fast as possible to by joined, only delayed by joined all
 *    threads it's currently managing first.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientListener(std::atomic<bool>& shutdown, std::atomic<uint16_t>& port);

}
