#pragma once

#include "config.hpp"
#include "sourceInfo.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * clientConnection
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Handles an entire client connection from start to finish, then closes
 *    itself.
 *
 * Takes:
 * -> client_sock:
 *    The open TCP socket connected to the client.
 * -> next_reader:
 *    An atomic counter that marks the next reader to use to distribute the
 *    read load around.
 * -> workers:
 *    The vector of database worker threads.
 * -> worker_stats:
 *    An array that corresponds to every threads current status. This is what
 *    this thread will use to flag its shutdown.
 * -> worker_strikes:
 *    An array that corresponds to every threads current failed responses.
 *    This thread will simply 0 this on startup.
 * -> read_workers:
 *    An array of the ports of the various read workers. If this thread is a
 *    read worker, its listening sockets port will be at
 *    read_workers[thread_ind].
 * -> write_worker:
 *    The port of the write workers listening socket.
 * -> election_mtx:
 *    The mutex to aquire a lock on to call an election. Any function including
 *    this one that attempts to modify db_workers in any way must aquire this
 *    lock to do so in a safe manner.
 * -> record_msgs:
 *    A flag that is true when messages are to be recorded.
 * -> record_queue:
 *    A q to record all variables when a database migration is occuring to prevent
 *    race conditions.
 * -> record_queue_mtx:
 *    The mutex for the record que
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void clientConnection(int                                              client_sock,
                      std::atomic<int>&                                next_reader,
                      std::array<std::thread,       WORKER_THREADS  >& workers,
                      std::array<std::atomic<bool>, WORKER_THREADS  >& worker_stats,
                      std::array<std::atomic<int>,  WORKER_THREADS  >& worker_strikes,
                      std::array<uint16_t,          WORKER_THREADS-1>& read_workers,
                      uint16_t&                                        write_worker,
                      std::mutex&                                      election_mtx,
                      std::vector<SourceInfo>&                         known_servers,
                      std::mutex&                                      known_server_mtx,
                      std::atomic<bool>&                               record_msgs,
                      std::queue<std::vector<uint8_t>>&                record_queue,
                      std::mutex&                                      record_queue_mtx);
}
