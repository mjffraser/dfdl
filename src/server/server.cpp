#include "server/server.hpp"
#include "server/internal/db.hpp"
#include "server/internal/serverThreads.hpp"
#include "sourceInfo.hpp"

#include <atomic>
#include <shared_mutex>
#include <thread>
#include <array>

namespace dfd {

std::atomic<bool> setup_okay = false;

void run_server(const std::string& ip,
                const uint16_t     port, 
                const std::string& connect_ip, 
                const uint16_t     connect_port) {
    SourceInfo our_server;
    our_server.ip_addr = ip;
    our_server.port    = port;

    //if we're connecting to another server, we need the info
    SourceInfo known_server;
    if (!connect_ip.empty()) {
        known_server.ip_addr = connect_ip;
        known_server.port    = connect_port;
    }

    //start database, establish db locks
    std::shared_mutex db_lock;
    std::atomic<bool> db_locking = false;
    Database* my_db = openDatabase("dfd-serv.db", db_lock, db_locking);

    //worker thread pool 
    std::array<std::thread, WORKER_THREADS> workers;
    
    //tracking for workers
    std::array<std::atomic<bool>, WORKER_THREADS>   worker_stats;       //worker working
    std::array<std::atomic<int>,  WORKER_THREADS>   worker_strikes;     //failure counter
    std::array<uint16_t,          WORKER_THREADS-1> read_workers;       //ports for read workers
    std::array<uint16_t,          WORKER_THREADS-1> election_listeners; //ports for election listeners

    //lock for election
    std::mutex election_lock;

    ///////////////////////////////////////////////////////////////////////////
    //STEP 1: STARTUP WORKER THREADS
    for (int i = 0; i < WORKER_THREADS; ++i) {
        if (i == WORKER_THREADS-1) //write thread
            workers[i] = std::thread(); 
    }
    
    




    closeDatabase(my_db);
}

}
