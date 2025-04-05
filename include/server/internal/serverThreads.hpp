#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <array>

#include "sourceInfo.hpp"

namespace dfd {

#define WORKER_THREADS 5

//forward declarations
class  Database;
struct SourceInfo;

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
 *    This is designed to be opened as a thread so requests to this database can
 *    still be received from other servers while the database is migrating. It's
 *    expected that no clients will be sending messages to this server while
 *    it's doing its initial registration with the network.
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

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * listenThread
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens a listening socket which will accept incoming client connections,
 *    and spawn internal threads to handle them via the database workers
 *    provided.
 *
 *    If this function fails at any point an error message is printed to stdout.
 *    No crash will occur.
 *
 *    This is designed to be opened as a thread.
 *
 * Takes:
 * -> port:
 *    The port to open on.
 * -> db_workers:
 *    The vector of database worker threads.
 * -> election_mtx:
 *    The mutex to aquire a lock on to call an election. Any function including
 *    this one that attempts to modify db_workers in any way must aquire this
 *    lock to do so in a safe manner.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void listenThread(const uint16_t            port,
                  std::vector<std::thread>& db_workers,
                  std::mutex&               election_mtx);


void workerThread(std::atomic<bool>&                             server_running,
                  int                                            thread_ind,
                  bool                                           writer,
                  std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                  std::array<uint16_t, WORKER_THREADS-1>&        read_workers,
                  std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                  uint16_t&                                      write_worker,
                  std::atomic<int>&                              setup_workers,
                  std::atomic<int>&                              setup_election_workers);


} //dfd
