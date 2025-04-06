#include "server/internal/serverThreads.hpp"
#include "networking/messageFormatting.hpp"
#include "server/internal/internal/electionThread.hpp"
#include "server/internal/internal/workerActions.hpp"
#include "networking/socket.hpp"
#include <cstdlib>
#include <iostream>

namespace dfd {

void joinNetwork(const SourceInfo&        known_server,
                 Database*                open_db,
                 std::vector<SourceInfo>& known_servers) {
    
}

void listenThread(std::atomic<bool>&        server_running,
                  const std::string&        ip,
                  const uint16_t            port,
                  std::vector<std::thread>& db_workers,
                  std::mutex&               election_mtx,
                  SourceInfo&               our_address) {
    ///////////////////////////////////////////////////////////////////////
    //SETUP PROCESS
    auto socket = openSocket(true, port);
    if (!socket) {
        std::cerr << "CRITICAL FAILURE, COULD NOT BIND LISTENER." << std::endl;
        return;
    }

    auto& [my_sock, _] = socket.value();

    if (EXIT_FAILURE == listen(my_sock, 10)) {
        std::cerr << "Could not start listening." << std::endl;
        closeSocket(my_sock);
        return;
    }

    ///////////////////////////////////////////////////////////////////////
    //MAIN LOOP
    while (server_running) {
        SourceInfo client;
        int client_sock = tcp::accept(my_sock, client);
        if (client_sock < 0) {
            continue;
        } else {
            std::thread();
        }
    }

    ///////////////////////////////////////////////////////////////////////
    //SHUTDOWN PROCESS
    closeSocket(my_sock);
}

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
                  Database*                                      db) {
    try {
        ///////////////////////////////////////////////////////////////////////
        //SETUP PROCESS
        //open sockets needed by worker
        auto listen_sock       = openSocket(true, 0, true); 
        auto election_listener = openSocket(true, 0, true);
        if (!listen_sock || !election_listener) {
            std::cerr << "COULD NOT OPEN WORKER" << std::endl;
            return;
        }
       
        //open election thread
        SourceInfo election_addr; election_addr.ip_addr = "127.0.0.1";
        uint16_t requester_port = 0; //election thread can access this address for the requesters port
        election_addr.port = election_listener.value().second;
        std::thread election_thread;
        std::atomic<bool> call_election = false;
        if (!writer) {
            election_thread = std::thread(electionThread, 
                                          std::ref(server_running),
                                          thread_ind,
                                          election_listener,
                                          std::ref(call_election),
                                          std::ref(requester_port),
                                          std::ref(worker_stats),
                                          std::ref(election_listeners),
                                          std::ref(setup_workers),
                                          std::ref(setup_election_workers));
            election_thread.detach();
        }

        //setup this thread
        worker_stats.at(thread_ind)   = true;
        worker_strikes.at(thread_ind) = 0;
        int my_socket = listen_sock.value().first;
        if (writer)
            write_worker = listen_sock.value().second;
        else
            read_workers[thread_ind] = listen_sock.value().second;

        //ensure all threads are setup before proceeding
        setup_workers++;
        int tries = 0;
        while (setup_workers          != WORKER_THREADS ||
               setup_election_workers != WORKER_THREADS-1) {
            tries++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (tries > 50) {
                return;
            }
        }

        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000;
        ///////////////////////////////////////////////////////////////////////
        //MAIN LOOP
        while ((server_running && worker_stats[thread_ind] == true) ||
               (listen_sock.value().second == write_worker)) {
            if (thread_ind == WORKER_THREADS-1 && listen_sock.value().second != write_worker)
                //if I wasn't responding fast enough and have been replaced
                break;
            else if (listen_sock.value().second == write_worker)
                //if i've been moved to write status
                thread_ind = WORKER_THREADS-1;

            SourceInfo msg_src;
            std::vector<std::uint8_t> client_request;
            int res = udp::recvMessage(my_socket, msg_src, client_request, timeout);
            call_election  = false;
            requester_port = 0;

            //no message
            if (res == EXIT_FAILURE)
                continue;

            //if I need to call election
            if (*client_request.begin() == ELECT_LEADER) {
                //signal to election 
                requester_port = msg_src.port;
                call_election = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
                continue;
            } else if (*client_request.begin() == ELECT_X ||
                       *client_request.begin() == LEADER_X) {
                continue;
            } 

            //handle message
            std::vector<uint8_t> response;
            switch (*client_request.begin()) {
                case INDEX_REQUEST:
                case INDEX_FORWARD: {
                    clientIndexRequest(client_request, response, db);
                    break;
                }

                case DROP_REQUEST:
                case DROP_FORWARD: {
                    clientDropRequest(client_request, response, db);
                }

                case REREGISTER_REQUEST:
                case REREGISTER_FORWARD: {
                    clientReregisterRequest(client_request, response, db);
                }

                case SOURCE_REQUEST: {
                    clientSourceRequest(client_request, response, db);
                }

                //SYNCING STUFF
                
                default: {
                    response = createFailMessage("Invalid message type.");
                }
            }

            udp::sendMessage(my_socket, msg_src, response);
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
