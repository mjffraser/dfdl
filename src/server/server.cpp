#include "server/server.hpp"
#include "server/internal/db.hpp"
#include "server/internal/serverThreads.hpp"
#include "server/internal/serverStartup.hpp"
#include "sourceInfo.hpp"

#include <atomic>
#include <iostream>
#include <shared_mutex>
#include <thread>
#include <array>
#include <unistd.h>
#include <queue>
#include <condition_variable>

namespace dfd {

std::atomic<bool> setup_okay = false;

void run_server(const std::string& ip,
                const uint16_t     port, 
                const std::string& connect_ip, 
                const uint16_t     connect_port) {
    SourceInfo our_address;
    our_address.ip_addr = ip;
    our_address.port    = port;

    //global vector storing pairs of known server IPs and ports
    //currently empty but by sending a source request via join network should do it
    //join network currently broke
    std::mutex              known_servers_mtx;
    std::vector<SourceInfo> known_servers;

    //globals related to the mass write send:
    //mass write send is a way to cover for potential requests which are recived during the database
    //migration proccess to a new server: this is a pair of a flag and a que which is filled when the flag is true
    std::atomic<bool> record_msgs = false;
    std::queue<std::vector<uint8_t>> record_queue;
    std::mutex record_queue_mtx;

    //if we're connecting to another server, we need the info
    SourceInfo target_server;
    if (!connect_ip.empty()) {
        target_server.ip_addr = connect_ip;
        target_server.port    = connect_port;
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
    uint16_t                                        write_worker;       //port for write worker

    //worker setup tracking
    std::atomic<int> setup_workers          = 0;
    std::atomic<int> setup_election_workers = 0;

    //lock and queue for controlMsg
    std::mutex                                  control_mtx;
    std::queue<std::pair<SourceInfo, uint64_t>> control_q;
    std::condition_variable                     control_cv;
    
    //lock for election
    std::mutex election_mtx;

    std::atomic<bool> server_running = true;
    std::atomic<uint16_t> election_requester = 0;
    ///////////////////////////////////////////////////////////////////////////
    //STEP 1: STARTUP WORKER THREADS AND CONTROL THREAD
    for (int i = 0; i < WORKER_THREADS; ++i) {
        bool is_write_thread;
        if (i == WORKER_THREADS-1) //write thread
            is_write_thread = true;
        else
            is_write_thread = false;
        workers[i] = std::thread(workerThread,
                                 std::ref(server_running),
                                 i,
                                 is_write_thread,
                                 std::ref(worker_stats),
                                 std::ref(worker_strikes),
                                 std::ref(read_workers),
                                 std::ref(election_listeners),
                                 std::ref(write_worker),
                                 std::ref(setup_workers),
                                 std::ref(setup_election_workers),
                                 my_db,
                                 std::ref(known_servers),
                                 std::ref(known_servers_mtx),
                                 std::ref(control_q),
                                 std::ref(control_cv),
                                 std::ref(control_mtx),
                                 std::ref(record_msgs), 
                                 std::ref(election_requester)); 
    }

    std::thread control_thread(controlMsgThread,
                               std::ref(server_running),
                               std::ref(control_q),
                               std::ref(control_cv),
                               std::ref(control_mtx),
                               std::ref(our_address));
    control_thread.detach();

    //pause to wait for setup to finish
    sleep(1);
    if (setup_workers          != WORKER_THREADS ||
        setup_election_workers != WORKER_THREADS-1) {
        std::cerr << "Setup is failing. Does this machine have the resources to be a server?" << std::endl;
        server_running = false;
        for (auto& w : workers)
            w.join(); 
        return;
    }

    ///////////////////////////////////////////////////////////////////////////
    //STEP 2: STARTUP LISTENER THREAD
    std::thread listen_thread(listenThread, 
                              std::ref(server_running),
                              std::ref(ip),
                              port,
                              std::ref(workers),
                              std::ref(worker_stats),
                              std::ref(worker_strikes),
                              std::ref(read_workers),
                              std::ref(write_worker),
                              std::ref(election_mtx),
                              std::ref(known_servers),
                              std::ref(known_servers_mtx),
                              std::ref(record_msgs),
                              std::ref(record_queue),
                              std::ref(record_queue_mtx));
    
    ///////////////////////////////////////////////////////////////////////////
    //STEP 3: (OPTIONALLY) CONNECT TO ANOTHER SERVER
    if (!target_server.ip_addr.empty())
        joinNetwork(target_server, my_db, known_servers, std::ref(known_servers_mtx), our_address);

    std::cout << "SERVER SETUP COMPLETE." << std::endl;
    ///////////////////////////////////////////////////////////////////////////
    //MAIN LOOP: POLL THREAD POOL EVERY 5s FOR FAILURES
    while (server_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::unique_lock<std::mutex> lock(election_mtx);
        for (int i = 0; i < WORKER_THREADS-1; ++i) {
            if (worker_stats[i]) continue;
            workers[i].join();
            setup_workers--;
            setup_election_workers--;
            std::cout << "RESTARTING THREAD: " << i << std::endl;
            workers[i] = std::thread(workerThread,
                                     std::ref(server_running),
                                     i,
                                     false,
                                     std::ref(worker_stats),
                                     std::ref(worker_strikes),
                                     std::ref(read_workers),
                                     std::ref(election_listeners),
                                     std::ref(write_worker),
                                     std::ref(setup_workers),
                                     std::ref(setup_election_workers),
                                     my_db,
                                     std::ref(known_servers),
                                     std::ref(known_servers_mtx),
                                     std::ref(control_q),
                                     std::ref(control_cv),
                                     std::ref(control_mtx),
                                     std::ref(record_msgs), 
                                     std::ref(election_requester)); 
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    //STEP 4: CLEANUP
    for (auto& w : workers) {
        w.join();
    }

    closeDatabase(my_db);
}

}
