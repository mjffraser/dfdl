#pragma once

#include "config.hpp"

#include <atomic>
#include <utility>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * electionThread
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A thread opened by every read thread that passively listens for either
 *    an in-election message from another thread, signalling to join, or a
 *    signal from its corresponding read_thread to kick off an election.
 *
 * Takes:
 * -> server_running:
 *    If this flag is false, this thread will make every effort to shutdown
 *    safely as soon as possible.
 * -> thread_ind:
 *    The index that refers to this threads' position in the workers thread
 *    pool maintained by the server. The last index corresponds to the write
 *    thread.
 * -> my_addr:
 *    The UDP port to use to listen, already created by my read thread.
 * -> call_election:
 *    An atomic boolean to poll. My read thread will signal the election with
 *    this.
 * -> requester_port:
 *    The port of socket on the thread that called the election.
 * -> election_listeners:
 *    A vector of every election thread's port.
 * -> setup_workers:
 *    A counter of how many worker threads have successfully set themselves up.
 *    This thread will not increment this, merely check it.
 * -> setup_election_workers:
 *    A counter of how many election threads have successfully set themselves
 *    up. This thread will only increment this counter once when it finishes
 *    setup.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void electionThread(std::atomic<bool>&                             server_running,
                    int                                            thread_ind,
                    std::pair<int, uint16_t>                       my_addr,
                    std::atomic<bool>&                             call_election,
                    std::atomic<uint16_t>&                         requester_port,
                    std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                    std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                    std::atomic<int>&                              setup_workers,
                    std::atomic<int>&                              setup_election_workers);

}
