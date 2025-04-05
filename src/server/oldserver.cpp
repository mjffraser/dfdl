//////declare stuff std::
#include <asm-generic/socket.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <array>
#include <poll.h>

//imports from project dfd::
#include "server/internal/syncing.hpp"
#include "sourceInfo.hpp"
#include "networking/socket.hpp"
#include "server/internal/db.hpp"
#include "networking/messageFormatting.hpp"

//other imports std::
#include <unistd.h>
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>

///////////////////////////////
//TODO:
//Thread protection, known servers
///////////////////////////////

namespace dfd {

//DATABASE
Database* db = nullptr;


//signals worker threads that a job is avalable
std::condition_variable job_ready;
//atomic flag to control main server loop (atomic prevents need of mutex)
std::atomic<bool> server_running(true);

//global vector storing pairs of known server IPs and ports
std::vector<SourceInfo> known_servers;
//our sockets Source info
SourceInfo our_server;

//ekko thread stuff
std::mutex ekko_mutex;
//when ekkoing is done this will be notified
std::condition_variable ekko_flag;

//Globals for the race condition resolution on server boot
//global sync flag (true if a server is being registered)
std::mutex syncing_mutex;
bool syncing_server = false;
//q for potential race condition msgs
std::mutex sync_queue_mutex;
std::queue<std::vector<uint8_t>> sync_message_queue;

#define WORKER_THREADS 10
std::atomic<int> setup_workers          = 0;
std::atomic<int> setup_election_workers = 0;
std::array<std::atomic<bool>, WORKER_THREADS> worker_stats;
std::array<std::atomic<int>,  WORKER_THREADS> worker_strikes;

std::vector<uint16_t> election_listeners;
std::vector<uint16_t> read_workers;
std::atomic<int>      next_reader = 0;

uint16_t              write_worker;
std::atomic<bool>     db_locking = false;
std::shared_mutex     db_lock;

std::mutex            election_lock;
std::atomic<uint16_t> election_requester = 0;

void controlMsgThread(SourceInfo faulty_client, uint64_t file_uuid) {
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
        return;
    }

    auto [sock_fd, my_port] = socket.value();

    for (int i = 0; i < num_of_attempts; i++){
        if (tcp::connect(sock_fd, faulty_client) != -1){
            client_reachable = true;
            break;
        }
    }

    // attempt to download one chunk from client
    while (client_reachable) {

        // check if file exist on client
        std::vector<uint8_t> control_msg = createDownloadInit(file_uuid, std::nullopt);
        if (EXIT_FAILURE == tcp::sendMessage(sock_fd, control_msg)) {
            std::cerr << "[controlMsgThread] Failed to send message to client.\n";
            closeSocket(sock_fd);
            return;
        }

        // expect client to ack back
        std::vector<uint8_t> request_ack;
        if (tcp::recvMessage(sock_fd, request_ack, timeout) < 0) {
            client_reachable = false;
            closeSocket(sock_fd);
            break;
        }

        // check for ack code
        if (request_ack.size() < 1 || request_ack[0] != DOWNLOAD_CONFIRM) {
            file_avalible = false;
            if (request_ack[0] == FAIL) {
                // error - client can't find file
            } 
            closeSocket(sock_fd);
            break;
        }

        // terminate file download
        tcp::sendMessage(sock_fd, {FINISH_DOWNLOAD});
        closeSocket(sock_fd);
        break;
    }

    if (!client_reachable || !file_avalible) {

        auto updete_sock = openSocket(false, 0, false);
        if (!updete_sock) {
            //handle fail to open
            std::cerr << "[controlMsgThread] Failed to open update socket\n";
            return;
        }

        auto [updete_fd, updete_port] = updete_sock.value();

        if (tcp::connect(updete_fd, our_server) < 0) {
            std::cerr << "[controlMsgThread] Failed to connect with our server.\n";
            closeSocket(updete_fd);
            return;
        }

        IndexUuidPair id_pair(file_uuid, faulty_client.peer_id);
        std::vector<uint8_t> drop_msg = createDropRequest(id_pair);
        if (EXIT_FAILURE == tcp::sendMessage(updete_fd,drop_msg)) {
            std::cerr << "[controlMsgThread] Failed to send message to our server.\n";
            closeSocket(updete_fd);
            return;
        }

        std::vector<uint8_t> update_respond;
        if (tcp::recvMessage(updete_fd, update_respond, timeout) < 0) {
            std::cerr << "[controlMsgThread] Failed to recv message from our server.\n";
            closeSocket(updete_fd);
            return;
        }

        if (update_respond[0] == FAIL) {
            std::cerr << "[controlMsgThread]" << parseFailMessage(update_respond) << std::endl;
        }

        closeSocket(updete_fd);
        return;


    }
}

void keepClientAlive(int client_fd,
                     std::atomic<bool>& run,
                     std::mutex& m,
                     std::condition_variable& cv) {
    while (run) {
        tcp::sendMessage(client_fd, {KEEP_ALIVE});
        std::unique_lock<std::mutex> lock(m);
        if (cv.wait_for(lock, std::chrono::milliseconds(500), [&run] {return !run;}))
            break;
    }

    return;
}

//these threads are spawned on each accepted connection and handle said connection

void handleConnectionThread(int client_fd, std::vector<std::thread>& workers) {
    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    std::vector<uint8_t> buffer; //msg buff

    //store the recved msg by calling it with the above time out n buffer
    ssize_t read = tcp::recvMessage(client_fd, buffer, timeout);
    //check if recv failed
    if (read <= 0) { //-1 on err
        std::cerr << "no message or timeout" << std::endl;
        closeSocket(client_fd);
        return;
    }

    auto udp_sock_init = openSocket(false, 0, true);
    if (!udp_sock_init) {
        closeSocket(client_fd);
        return;
    }

    std::atomic<bool> still_running = true;
    std::mutex keep_alive_mtx;
    std::condition_variable keep_alive_cv;
    std::thread keep_alive(keepClientAlive, client_fd, std::ref(still_running), std::ref(keep_alive_mtx), std::ref(keep_alive_cv));

    int udp_sock = udp_sock_init.value().first;
    for (int i = 0; i < 10; ++i) {
        SourceInfo worker_addr; worker_addr.ip_addr = "127.0.0.1";
        int worker_id;
        if (*buffer.begin() == SOURCE_REQUEST) {
            //if a race happens and multiple threads use this reader should still be fine
            //a thread can handle many requests quickly, just spread out the requests to
            //improve load
            worker_id = next_reader;
            next_reader = (next_reader + 1) % (WORKER_THREADS-1); 
            if (!worker_stats[worker_id] || read_workers[worker_id] == 0)
                continue;
            worker_addr.port = read_workers[worker_id];
        } else {
            worker_id = WORKER_THREADS-1; //last id always corresponds to writer
            worker_addr.port = write_worker;
            //this bit here adds msg to sync msg q if were syncing to help with race conditions when sending server
            {
                std::lock_guard<std::mutex> lock(syncing_mutex);
                if (syncing_server) {
                        std::lock_guard<std::mutex> qlock(sync_queue_mutex);
                        sync_message_queue.push(buffer);
                }
            }
        }

        std::cout << "attempt " << i << std::endl;
        if (EXIT_FAILURE == udp::sendMessage(udp_sock, worker_addr, buffer)) {
            worker_strikes[worker_id]++;
            continue;
        }

        std::vector<uint8_t> response;
        struct timeval timeout_udp;
        timeout_udp.tv_sec  = 0;
        timeout_udp.tv_usec = 500000;

        if (EXIT_SUCCESS == udp::recvMessage(udp_sock, worker_addr, response, timeout_udp)) {
            if (worker_strikes[worker_id] > 0)
                worker_strikes[worker_id] = 0;

            if (*buffer.begin() == SERVER_REG) {
                databaseSendNS(client_fd);

                //stop recording
                {
                    std::lock_guard<std::mutex> lock(syncing_mutex);
                    syncing_server = false;
                }
            }

            {
                std::lock_guard<std::mutex> lock(keep_alive_mtx);
                still_running = false;
            }
            keep_alive_cv.notify_one();
            keep_alive.join();

            tcp::sendMessage(client_fd, response);
            closeSocket(client_fd);

            if (*buffer.begin() == SERVER_REG) {
                SourceInfo si = parseNewServerReg(buffer);
                // forward queued updates to the new server
                dfd::massWriteSend(si, sync_message_queue);
                {
                    std::lock_guard<std::mutex> lock(sync_queue_mutex);
                    //empty q so it works next time
                    sync_message_queue = std::queue<std::vector<uint8_t>>();
                }
            }

            return;
        }

        worker_strikes[worker_id]++;

        //we check if we need to mark off worker/call election
        //read thread
        if (worker_id != WORKER_THREADS-1) {
            if (worker_strikes[worker_id] >= 5) {
                worker_stats[worker_id]   = false;
                worker_strikes[worker_id] = 0;
            }
        }

        //write thread (election)
        else {
            struct timeval election_timeout;
            election_timeout.tv_sec = 0;
            election_timeout.tv_usec = 250000;
            if (worker_strikes[worker_id] >= 5) {
                std::unique_lock<std::mutex> lock(election_lock, std::try_to_lock); 
                if (!lock.owns_lock())
                    continue; //if somebody else is calling election we go to next try
                worker_stats[worker_id] = false;

                std::vector<uint8_t> election_buff = {ELECT_LEADER};
                SourceInfo reader_addr;
                reader_addr.ip_addr = "127.0.0.1"; reader_addr.port = read_workers[next_reader];
                udp::sendMessage(udp_sock, reader_addr, election_buff);
                std::vector<uint8_t> response;
                udp::recvMessage(udp_sock, reader_addr, response, election_timeout);
                if (response.size() > 0 && *response.begin() == LEADER_X) {
                    int leader_ind = (int)response[1];
                    std::cout << "LEADER IS " << leader_ind << std::endl;

                    //we set leader, informing all threads of the new leader if needed 
                    worker_stats[leader_ind]  = false;
                    write_worker              = read_workers[leader_ind];
                    read_workers[leader_ind]  = 0;
                    worker_strikes[worker_id] = 0;
                    worker_stats[worker_id]   = true;

                    //for spawning a new read thread later
                    std::swap(workers[leader_ind], workers[WORKER_THREADS-1]);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(keep_alive_mtx);
        still_running = false;
    }
    keep_alive_cv.notify_one();
    keep_alive.join();

    std::vector<uint8_t> fail_msg = createFailMessage("Database appears to not be working. Sorry, please try another server.");
    tcp::sendMessage(client_fd, fail_msg);
    closeSocket(client_fd);
}

void electionThread(int thread_ind, 
                    std::pair<int, uint16_t> election_listener,
                    std::atomic<bool>&       call_election,
                    uint16_t my_worker_port) {
    int listener = election_listener.first;
    election_listeners.at(thread_ind) = election_listener.second;

    //setup
    setup_election_workers++;
    int tries = 0;
    while (setup_workers          != WORKER_THREADS ||
           setup_election_workers != WORKER_THREADS-1) {
        tries++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (tries > 50) {
            return;
        }
    }

    struct timeval response_timeout;
    response_timeout.tv_sec  = 0;
    response_timeout.tv_usec = 100;
    struct timeval polling_timeout;
    polling_timeout.tv_sec  = 0;
    polling_timeout.tv_usec = 500;
    struct timeval clearing_timeout;
    clearing_timeout.tv_sec  = 0;
    clearing_timeout.tv_usec = 10;

    bool in_election = false;
    while (server_running && worker_stats[thread_ind]) {
        if (call_election || in_election) {
            in_election   = true;
            call_election = false;
            while (in_election) {
                for (int i = thread_ind+1; i < WORKER_THREADS-1; ++i) {
                    SourceInfo dest; dest.ip_addr = "127.0.0.1";
                    dest.port = election_listeners[i];
                    udp::sendMessage(listener, dest, {ELECT_X, (uint8_t)thread_ind});
                }

                SourceInfo dest;
                std::vector<uint8_t> response;
                int res = udp::recvMessage(listener, dest, response, response_timeout);
                if (res == EXIT_FAILURE) {
                    in_election = false;
                    SourceInfo asker; asker.ip_addr         = "127.0.0.1"; asker.port     = election_requester;
                    SourceInfo my_worker; my_worker.ip_addr = "127.0.0.1"; my_worker.port = my_worker_port;
                    udp::sendMessage(listener, asker, {LEADER_X, (uint8_t)thread_ind});
                    udp::sendMessage(listener, my_worker, {LEADER_X});
                } else if (res == EXIT_SUCCESS) {
                    //if we got a message
                    if (*response.begin() == BULLY) { //we're being bullied, so we end our leader contention
                        in_election = false;
                        while (EXIT_SUCCESS == udp::recvMessage(listener, dest, response, clearing_timeout)) {
                            //clear queued messages from this election
                        }
                        break;

                    } else if (*response.begin() == ELECT_X) {
                        //condition isn't strictly needed, should never get a ELECT_X from higher id
                        if ((int)response[1] < thread_ind) {
                            udp::sendMessage(listener, dest, {BULLY});
                        }
                    }
                }
            }
        } else {
            SourceInfo sender;
            std::vector<uint8_t> message;
            int res = udp::recvMessage(listener, sender, message, polling_timeout);

            //if we received something
            if (res == EXIT_SUCCESS) {
                //if they sent something we care about
                if (*message.begin() == ELECT_X) {
                    //bully sender as they're sending to higher ids
                    udp::sendMessage(listener, sender, {BULLY});
                    in_election = true;
                }
            }
        } 
    }

    return;
}

//worker thread
void workerThread(int thread_ind, bool writer=false) {
    try {
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

        //setup worker stats
        worker_stats.at(thread_ind)   = true;
        worker_strikes.at(thread_ind) = 0;
        int my_socket = listen_sock.value().first;
        if (writer)
            write_worker = listen_sock.value().second;
        else
            read_workers[thread_ind] = listen_sock.value().second;

        //make sure every other thread is setup 
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

        // if (thread_ind == 4 || thread_ind == 8)
        //     throw std::bad_alloc();

        struct timeval timeout;
        timeout.tv_sec  = 0;
        timeout.tv_usec = 50000;
        while ((server_running && worker_stats[thread_ind] == true) ||
               (listen_sock.value().second == write_worker)) {
            if (thread_ind == WORKER_THREADS-1 && listen_sock.value().second != write_worker)
                break;
            else if (listen_sock.value().second == write_worker)
                thread_ind = WORKER_THREADS-1;

            SourceInfo msg_src;
            std::vector<std::uint8_t> buff;
            int res = udp::recvMessage(my_socket, msg_src, buff, timeout);
            call_election = false;

            if (res == EXIT_FAILURE)
                continue;

            if (*buff.begin() == ELECT_LEADER) {
                election_requester = msg_src.port;
                call_election = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
                continue;
            } else if (*buff.begin() == ELECT_X ||
                       *buff.begin() == LEADER_X) {
                continue;
            } 

            std::vector<uint8_t> response;
            switch (*buff.begin()) {
                case INDEX_REQUEST: {
                    FileId file_id = parseIndexRequest(buff);
                    auto failed_servers = forwardIndexRequest(buff, known_servers);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->indexFile(file_id.uuid, 
                                                      file_id.indexer, 
                                                      file_id.f_size,
                                                      db_lock))
                        response = createFailMessage(db->sqliteError()); 
                    else
                        response = {INDEX_OK}; //ack-byte
                    db_locking = false;

                    if (!failed_servers.empty()){
                        removeFailedServers(known_servers, failed_servers);
                    }
                    break;
                }

                case INDEX_FORWARD: {
                    FileId file_id = parseIndexRequest(buff);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->indexFile(file_id.uuid, 
                                                      file_id.indexer, 
                                                      file_id.f_size,
                                                      db_lock))
                        response = createFailMessage(db->sqliteError()); 
                    else
                        response = {FORWARD_OK}; //ack-byte
                    db_locking = false;
                    break;
                }

                case DROP_REQUEST: {
                    // if (writer) continue;

                    //see messageFormatting for IndexUuidPair
                    IndexUuidPair uuids = parseDropRequest(buff);
                    auto failed_servers = forwardDropRequest(buff, known_servers);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->dropIndex(uuids.first, uuids.second, db_lock))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {DROP_OK};
                    db_locking = false;
                    
                    if (!failed_servers.empty()){
                        removeFailedServers(known_servers, failed_servers);
                    }
                    break;
                }
                case DROP_FORWARD: {
                    //see messageFormatting for IndexUuidPair
                    IndexUuidPair uuids = parseDropRequest(buff);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->dropIndex(uuids.first, uuids.second, db_lock))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {FORWARD_OK};
                    db_locking = false;
                    break;
                }

                case REREGISTER_REQUEST: {
                    SourceInfo client_info = parseReregisterRequest(buff);
                    auto failed_servers = forwardReregRequest(buff, known_servers);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->updateClient(client_info, db_lock))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {REREGISTER_OK};
                    db_locking = false;
                    
                    if (!failed_servers.empty()){
                        removeFailedServers(known_servers, failed_servers);
                    }
                    break;
                }
                case REREGISTER_FORWARD: {
                    SourceInfo client_info = parseReregisterRequest(buff);
                    db_locking = true;
                    if (EXIT_SUCCESS != db->updateClient(client_info, db_lock))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {FORWARD_OK};
                    db_locking = false;
                    break;   
                }

                case SOURCE_REQUEST: {
                    uint64_t f_uuid = parseSourceRequest(buff);
                    std::vector<SourceInfo> indexers;
                    if (EXIT_SUCCESS != db->grabSources(f_uuid, indexers, db_locking, db_lock))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = createSourceList(indexers);
                    break;
                }

                case SERVER_REG: {
                    //start recording msgs to prevent race cond
                    {
                        std::lock_guard<std::mutex> lock(syncing_mutex);
                        syncing_server = true;
                    }
                    std::cout << "registering with " << known_servers.size() << std::endl;
                    SourceInfo new_server = parseNewServerReg(buff);
                    ssize_t registered_with = forwardRegistration(buff, known_servers);
                    if (registered_with < 0) {
                        response = createFailMessage("I appear to be a dead server myself?");
                    } else {
                        response = createServerRegResponse(known_servers);
                        known_servers.push_back(new_server);
                        db->backupDatabase("temp.db");
                    }
                    break;
                }

                case FORWARD_SERVER_REG: {
                    SourceInfo new_server = parseForwardServerReg(buff);
                    known_servers.push_back(new_server);
                    response = {FORWARD_SERVER_OK};
                    break;
                }

                case CLIENT_REG: {
                    response = createServerRegResponse(known_servers);
                    break;
                }

                case CONTROL_REQUEST : {
                    auto [file_uuid, faulty_client] = parseControlRequest(buff);
                    if (faulty_client.port == 0) {
                        response = createFailMessage("Invalid message.");
                    } else {
                        std::thread control_thread(controlMsgThread, faulty_client, file_uuid);
                        control_thread.detach();
                        response = {CONTROL_OK};
                    }
                    break;
                }

                default: {
                    std::cout << (int)*buff.begin() << " " << thread_ind << std::endl; 
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
        worker_stats.at(thread_ind) = false;
        return;
    }

    return;
}


void listenThread(const uint16_t port, std::vector<std::thread>& workers) {
    auto socket = openSocket(true, port);
    if (!socket) {
        //handle fail to open
        std::cerr << "no socket for socket thread";
        return;
    }

    //server file descriptor
    int server_fd = socket->first;
    std::cout << "listening on port " << socket->second << "\n\n";

    //start listening with backlog of 10 (number is unimportant)
    if (listen(server_fd, 10) == EXIT_FAILURE) {
        std::cerr << "listen failed\n";
        closeSocket(server_fd);
        return;
    }

    //////////////////
    //TODL:
    //errorhandle below, make IP default to local host, midhigh prio
    //////////////////
    //set our own server info
    our_server.ip_addr = "127.0.0.1"; //TODO: FAULT TOLERANCE
    our_server.port = port;

    while (server_running) {
        SourceInfo clientInfo;
        
        //accept new client connection
        int client_sock = tcp::accept(server_fd, clientInfo);
        
        if (client_sock < 0) {
            std::cerr << "Client disconnected.\n";
        } else {
            std::cout << "Served client: " << clientInfo.ip_addr << ":" << clientInfo.port << "\n";
            //spawn a thread that handles the connection
            //keep in mind detached threads can be bad we may want a threadpool
            std::thread(handleConnectionThread, client_sock, std::ref(workers)).detach();
        }
    }
    
    //close up socket with api
    closeSocket(server_fd);
}

void setupThread(SourceInfo known_server) {
    // Extract IP and port
    std::string server_ip = known_server.ip_addr;
    uint8_t server_port   = known_server.port;

    //open client TCP socket (unsure if server_port is right or if I should default this to somethin)
    auto socket = openSocket(false, server_port);
    if (!socket) {
        std::cerr << "Failed to open client socket for setup.\n";
        return;
    }

    //our client socket we are using
    int client_sock = socket.value().first;

    //attempt to connect and catch any errors and output error
    if (tcp::connect(client_sock, known_server) == EXIT_FAILURE) {
        std::cerr << "couldent connect to sister server @ IP:" << server_ip << "PORT:" << server_port << "\n";
        closeSocket(client_sock);
        return;
    }
    //connection successful
    std::cout << "connected to sister server @ IP:" << server_ip << "PORT:" << server_port << "\n";

    //send initial_message setup request message
    std::vector<uint8_t> setup_message = createNewServerReg(our_server);
    if (tcp::sendMessage(client_sock, setup_message) == EXIT_FAILURE) {
        std::cerr << "Failed to send setup message.\n";
        closeSocket(client_sock);
        return;
    }

    //recive the DB from known server
    if (databaseReciveNS(client_sock, db) != EXIT_SUCCESS) {
        std::cerr << "db failed to be obtained and merged in setup\n";
    } else {
        std::cout << "db obtained and merged in setup\n";
    }

    //buffer for response
    std::vector<uint8_t> buffer;
    timeval timeout = {5, 0};
    ssize_t read_bytes = tcp::recvMessage(client_sock, buffer, timeout);
    //errorcheck
    if (read_bytes <= 0) {
        std::cerr << "no response from known server.\n";
        closeSocket(client_sock);
        return;
    }

    //known_servers = all known severs of connected server+the connected server
    known_servers = parseServerRegResponse(buffer);
    known_servers.push_back(known_server);

    std::cout << "Registered with server network." << std::endl;
    std::cout << "[DEBUG] SERVERS:" << std::endl;
    for (auto& s : known_servers) {
        std::cout << s.ip_addr << " " << s.port << std::endl;
    }

    //close socket
    closeSocket(client_sock);
}


///////main function
int run_server(const uint16_t     port, 
               const std::string& connect_ip, 
               const uint16_t     connect_port) {
    //if we're connecting somewhere to register as a new server
    SourceInfo known_server;
    if (!connect_ip.empty()) {
        known_server.ip_addr = connect_ip;
        known_server.port    = connect_port;
    }
    
    //open database
    // db = new Database("name.db");

    //output msg
    std::cout << "DB setup complete." << std::endl;

    //vector of threads containing our workers
    std::vector<std::thread> workers;
    
    //create worker threads
    read_workers.resize(WORKER_THREADS-1);
    election_listeners.resize(WORKER_THREADS-1);
    for (int i = 0; i < WORKER_THREADS; ++i) {
        if (i == WORKER_THREADS-1)
            workers.push_back(std::thread(workerThread, i, true));
        else
            workers.push_back(std::thread(workerThread, i, false));
    }

    //ensure all worker threads setup their ports properly
    sleep(1);
    if (setup_workers          != WORKER_THREADS ||
        setup_election_workers != WORKER_THREADS-1) {
        std::cerr << "Setup is failing. Does this machine have the resources to be a server?" << std::endl;
        server_running = false;
        for (auto& w : workers)
            w.join(); 
        return EXIT_FAILURE;
    }

    //start the listen thread
    //all new connections are received here, and handed off for processing
    std::thread listener_thread(listenThread, port, std::ref(workers));

    std::cout << "Internal setup complete" << std::endl;

    //start setup thread with known server *only if a known server was provided*
    std::thread setup_thread;
    if (!known_server.ip_addr.empty()) {
        setup_thread = std::thread(setupThread, known_server);
        setup_thread.join();
    }

    //worker restart
    while (server_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        std::unique_lock<std::mutex> lock(election_lock); //dont restart while election taking place
        for (int i = 0; i < WORKER_THREADS-1; ++i) {
            if (!worker_stats.at(i)) { //thread died for any reason
                std::cout << "TRYING TO RESTART " << i << std::endl;
                workers[i].join();
                std::cout << "RESTARTING THREAD " << i << std::endl;
                setup_workers--;
                setup_election_workers--;
                workers[i] = std::thread(workerThread, i, false); 
            }
        }
    }

    ///////////CLEANUP///////////
    //stop listener
    listener_thread.join();
    std::cout << "Cleaning up...\n";
    //signal workers to be done
    server_running = false;
    //notify every worker (since server running is false this will break them all out of loop)
    job_ready.notify_all();
    
    // iterate through vector end all workers (after waiting for em to finish via join)
    for (int i = 0; i < workers.size(); i++)
        workers[i].join();

    //close db
    delete db;
    
    std::cout << "Cleanup complete. Server shutting down...\n";
    return 0;
}

}//dfd
