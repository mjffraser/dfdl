#include "server/internal/serverThreads.hpp"
#include "networking/messageFormatting.hpp"
#include "server/internal/internal/electionThread.hpp"
#include "server/internal/internal/workerActions.hpp"
#include "server/internal/internal/clientConnection.hpp"
#include "server/internal/syncing.hpp"
#include "networking/socket.hpp"

#include <cstdlib>
#include <iostream>
#include <queue>
#include <thread>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>

namespace dfd {

void controlMsgThread(std::atomic<bool>&                           server_running,
                      std::queue<std::pair<SourceInfo, uint64_t>>& control_q,
                      std::condition_variable&                     control_cv,
                      std::mutex&                                  control_mtx,
                      SourceInfo&                                  our_server) {
    while(server_running) {
        SourceInfo  faulty_client;
        uint64_t    file_uuid;
        {
            std::unique_lock<std::mutex> lock(control_mtx);
            control_cv.wait(lock, [&] {return !control_q.empty() || !server_running;});

            if (!server_running) {
                break;
            } else {
                auto [bad_client, uuid] = control_q.front();
                control_q.pop();
                faulty_client = bad_client;
                file_uuid = uuid;
            }
        }

        int num_of_attempts = 3;
        bool client_reachable = false;
        bool file_avalible = true;

        struct timeval timeout;
        timeout.tv_sec  = 5;
        timeout.tv_usec = 0;

        auto socket = openSocket(false, 0, false);
        if (!socket) {
            //handle fail to open
            std::cerr << "[controlMsgThread] Failed to open socket\n";
            continue;
        }

        auto [sock_fd, my_port] = socket.value();

        for (int i = 0; i < num_of_attempts; i++){
            if (tcp::connect(sock_fd, faulty_client) != -1){
                client_reachable = true;
                break;
            }
        }

        if (!client_reachable || !file_avalible) {
            auto updete_sock = openSocket(false, 0, false);
            if (!updete_sock) {
                //handle fail to open
                std::cerr << "[controlMsgThread] Failed to open update socket\n";
                break;
            }

            auto [updete_fd, updete_port] = updete_sock.value();

            if (tcp::connect(updete_fd, our_server) < 0) {
                std::cerr << "[controlMsgThread] Failed to connect with our server.\n";
                closeSocket(updete_fd);
                break;
            }

            std::cout << "SENDING DROP TO " << our_server.ip_addr << ":" << our_server.port << std::endl;
            std::cout << "FAULTY INFO: " << file_uuid << " " << faulty_client.peer_id << std::endl;
            IndexUuidPair id_pair(file_uuid, faulty_client.peer_id);
            std::vector<uint8_t> drop_msg = createDropRequest(id_pair);
            if (EXIT_FAILURE == tcp::sendMessage(updete_fd,drop_msg)) {
                std::cerr << "[controlMsgThread] Failed to send message to our server.\n";
                closeSocket(updete_fd);
                break;
            }

            std::vector<uint8_t> update_respond;
            if (tcp::recvMessage(updete_fd, update_respond, timeout) < 0) {
                std::cerr << "[controlMsgThread] Failed to recv message from our server.\n";
                closeSocket(updete_fd);
                break;
            }

            if (update_respond[0] == FAIL) {
                std::cerr << "[controlMsgThread]" << parseFailMessage(update_respond) << std::endl;
            }

            closeSocket(updete_fd);
        }
    }
}

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
                  std::mutex&                                      known_servers_mtx,
                  std::atomic<bool>&                               record_msgs,
                  std::queue<std::vector<uint8_t>>&                record_queue,
                  std::mutex&                                      record_queue_mtx) {
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

    std::atomic<int> next_reader = 0;
    ///////////////////////////////////////////////////////////////////////
    //MAIN LOOP
    while (server_running) {
        SourceInfo client;
        int client_sock = tcp::accept(my_sock, client);
        if (client_sock < 0) {
            continue;
        } else {
            std::thread client_conn(clientConnection,
                                    client_sock,
                                    std::ref(next_reader),
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
            client_conn.detach();
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
                  Database*                                      db,
                  std::vector<SourceInfo>&                       known_servers,
                  std::mutex&                                    knowns_mtx,
                  std::queue<std::pair<SourceInfo, uint64_t>>&   control_q,
                  std::condition_variable&                       control_cv,
                  std::mutex&                                    control_mtx,
                  std::atomic<bool>&                             record_msgs,
                  std::atomic<uint16_t>&                         election_requester) {
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
        election_addr.port = election_listener.value().second;
        std::thread election_thread;
        std::atomic<bool> call_election = false;
        if (!writer) {
            election_thread = std::thread(electionThread, 
                                          std::ref(server_running),
                                          thread_ind,
                                          election_listener.value(),
                                          std::ref(call_election),
                                          std::ref(election_requester),
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

        size_t writes_served = 0;

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

            if (writes_served > 2) return; //manually trigger leader election

            SourceInfo msg_src;
            std::vector<std::uint8_t> client_request;
            int res = udp::recvMessage(my_socket, msg_src, client_request, timeout);
            call_election  = false;

            //no message
            if (res == EXIT_FAILURE)
                continue;

            //if I need to call election
            if (*client_request.begin() == ELECT_LEADER) {
                //signal to election 
                election_requester = msg_src.port;
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
                //NOTE: added breaks to all cases cause was not sure if needed
                case INDEX_REQUEST: 
                case INDEX_FORWARD: {
                    clientIndexRequest(client_request, response, db);
                    writes_served++;
                    break;
                }

                case DROP_REQUEST:
                case DROP_FORWARD: {
                    clientDropRequest(client_request, response, db);
                    writes_served++;
                    break;
                }

                case REREGISTER_REQUEST: 
                case REREGISTER_FORWARD: {
                    clientReregisterRequest(client_request, response, db);
                    writes_served++;
                    break;
                }

                case SOURCE_REQUEST: {
                    clientSourceRequest(client_request, response, db);
                    break;
                }

                //SYNCING STUFF
                case SERVER_REG: {
                    serverToServerRegistration(client_request, response, known_servers, knowns_mtx, db, record_msgs);
                    break;
                }

                //keeping here as its just so simple
                case FORWARD_SERVER_REG: {
                    SourceInfo new_server = parseForwardServerReg(client_request);
                    {
                        std::lock_guard<std::mutex> lock(knowns_mtx);
                        known_servers.push_back(new_server);
                    }
                    response = {FORWARD_SERVER_OK};
                    break;
                }

                case CLIENT_REG: {
                    {
                        std::lock_guard<std::mutex> lock(knowns_mtx);
                        response = createServerRegResponse(known_servers);
                    }
                    break;
                }
                
                case CONTROL_REQUEST: {
                    auto [file_uuid, faulty_client] = parseControlRequest(client_request);
                    std::cout << "YAYAYA" << faulty_client.ip_addr << " " << file_uuid << std::endl;
                    if (faulty_client.port == 0) {
                        response = createFailMessage("Invalid message.");
                    } else {
                        {
                            std::lock_guard<std::mutex> lock(control_mtx);
                            control_q.push({faulty_client, file_uuid});
                        }
                        control_cv.notify_one();
                        response = {CONTROL_OK};
                    break;
                    }
                }
                
                default: {
                    response = createFailMessage("Invalid message type.");
                }
            }

            if (writer) std::cout << "WRITES PERFORMED: " << writes_served << std::endl;
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
