#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
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
 * -> server_running:
 *    An atomic flag for if the server should enter the shutdown phase, and this
 *    function should exit as soon as possible.
 * -> ip:
 *    Not currently used. For syncing.
 * -> port:
 *    The port to open on.
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void listenThread(std::atomic<bool>&                               server_running,
                  const std::string&                               ip,
                  const uint16_t                                   port,
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

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * workerThread
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens a listening socket which will accept incoming client requests
 *    buffered by their respective client threads. Also opens an election thread
 *    that corresponds to this worker, and will conduct elections among the
 *    threads when needed. This thread will carry out client requests to the
 *    database and return the reply back to the client handling thread.
 *
 *    This is designed to be opened as a thread.
 *
 *    If this function fails at an point it will attempt to mark itself for
 *    restart before doing so. Any non-OS error will be caught and this
 *    thread will shut itself down and mark for restart.
 *
 * Takes:
 * -> server_running:
 *    If this flag is false, this thread will make every effort to shutdown
 *    safely as soon as possible.
 * -> thread_ind:
 *    The index that refers to this threads' position in the workers thread
 *    pool maintained by the server. The last index corresponds to the write
 *    thread.
 * -> writer:
 *    If this thread was opened as a writer.
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
 * -> election_listeners:
 *    An array similar to the above, but for the election thread listeners
 *    instead.
 * -> write_worker:
 *    The port of the write workers listening socket.
 * -> setup_workers:
 *    A counter of how many worker threads have successfully set themselves up.
 *    This thread should only increment this counter once.
 * -> setup_election_workers:
 *    A counter of how many election threads have successfully set themselves
 *    up. This thread should pass this to its election thread on creation for
 *    it to increment.
 * -> db:
 *    The Database class instance for this server.
 * -> known_servers:
 *    Vector of known servers.
 * -> known_server_mtx:
 *    Mutex for known servers.
 * -> control_q, control_cv, control_mtx:
 *    The control que and its conditional variable for access as well as a mutex to protect it.
 * -> record_msgs:
 *    A flag that is true when messages are to be recorded.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void workerThread(std::atomic<bool>&                             server_running,
                  int                                            thread_ind,
                  bool                                           writer,
                  std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                  std::array<std::atomic<int>,  WORKER_THREADS>& worker_strikes, 
                  std::array<uint16_t, WORKER_THREADS-1>&        read_workers,
                  std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                  uint16_t&                                      write_worker,
                  std::atomic<int>&                              setup_workers,
                  std::atomic<int>&                              setup_election_workers,
                  Database*                                      db,
                  std::vector<SourceInfo>&                       known_servers,
                  std::mutex&                                    knowns_mtx,
                  std::queue<std::pair<SourceInfo, uint64_t>>&   control_q,
                  std::condition_variable&                       control_cv,
                  std::mutex&                                    control_mtx,
                  std::atomic<bool>&                             record_msgs,
                  std::atomic<uint16_t>&                         election_requester);

/*
*
*/
void controlMsgThread(std::atomic<bool>&                           server_running,
                      std::queue<std::pair<SourceInfo, uint64_t>>& control_q,
                      std::condition_variable&                     control_cv,
                      std::mutex&                                  control_mtx,
                      SourceInfo&                                  our_server);


} //dfd
