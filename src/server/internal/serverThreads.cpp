#include "server/internal/serverThreads.hpp"
#include "networking/socket.hpp"
#include <iostream>

namespace dfd {

void workerThread(std::atomic<bool>&                             server_running,
                  int                                            thread_ind,
                  bool                                           writer,
                  std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                  std::array<uint16_t, WORKER_THREADS-1>&        read_workers,
                  std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                  uint16_t&                                      write_worker,
                  std::atomic<int>&                              setup_workers,
                  std::atomic<int>&                              setup_election_workers) {
    try {
        //open sockets needed by worker
        auto listen_sock       = openSocket(true, 0, true); 
        auto election_listener = openSocket(true, 0, true);
        if (!listen_sock || !election_listener) {
            std::cerr << "COULD NOT OPEN WORKER" << std::endl;
            return;
        }
        
        SourceInfo election_addr; election_addr.ip_addr = "127.0.0.1";
        election_addr.port = election_listener.value().second;
        std::thread election_thread;
        std::atomic<bool> call_election = false;
        if (!writer) {
            election_thread = std::thread(electionThread, 
                                          thread_ind,
                                          election_listener.value(),
                                          std::ref(call_election),
                                          listen_sock.value().second);
            election_thread.detach();
        }

    } catch (...) {
        //we catch any crash and just return
        //EXCEPT FOR MEMORY ERRORS. IF MEMORY READS/WRITES ARE ACTING ERRONEOUSLY,
        //IT'S VERY DANGEROUS TO KEEP OPERATING, AND WE LET THE ENTIRE SERVER CRASH.
        //
        //Any error like a segfault is an implementation error, and should be addressed
        //before re-deploying server.
        worker_stats[thread_ind] = false;
        return;
    }
}

}
