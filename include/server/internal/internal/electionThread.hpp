#pragma once

#include "config.hpp"

#include <atomic>
#include <utility>

namespace dfd {

void electionThread(std::atomic<bool>&                             server_running,
                    int                                            thread_ind,
                    std::pair<int, uint16_t>                       my_addr,
                    std::atomic<bool>&                             call_election,
                    uint16_t&                                      requester_port,
                    std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                    std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                    std::atomic<int>&                              setup_workers,
                    std::atomic<int>&                              setup_election_workers);

}
