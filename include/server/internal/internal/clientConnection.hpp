#pragma once

#include "config.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <thread>

namespace dfd {

void clientConnection(int                                              client_sock,
                      std::atomic<int>&                                next_reader,
                      std::array<std::thread,       WORKER_THREADS  >& workers,
                      std::array<std::atomic<bool>, WORKER_THREADS  >& worker_stats,
                      std::array<std::atomic<int>,  WORKER_THREADS  >& worker_strikes,
                      std::array<uint16_t,          WORKER_THREADS-1>& read_workers,
                      uint16_t&                                        write_worker,
                      std::mutex&                                      election_mtx);

}
