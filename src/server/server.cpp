//////declare stuff std::
#include <iostream>
#include <thread>

//imports from project dfd::
#include "server/internal/syncing.hpp"
#include "sourceInfo.hpp"
#include "networking/socket.hpp"
#include "server/internal/database/db.hpp"
#include "networking/messageFormatting.hpp"

//other imports std::
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace dfd {

//DATABASE
std::mutex db_mutex;
Database* db = nullptr;

struct Job {
    //file descriptor of the client
    int client_sock;
    //message sent by the client
    std::vector<uint8_t> client_message;
};

//queue to hold pending jobs and its mutex
std::queue<Job> job_q;
std::mutex job_mutex;

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


//these threads are spawned on each accepted connection and handle said connection
void handleConnectionThread(int client_fd) {
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

    {
        std::lock_guard<std::mutex> lock(job_mutex);
        //create a job on the worker queue with this connections file descriptor and buffer containing msg
        job_q.push({client_fd, buffer});
    }
    
    //notify workers that a job is avalable
    job_ready.notify_one();
}

//worker thread, needs message handelling
void workerThread() {
    while (server_running) {
        //the job this thread is working on
        Job job;
        //mutex lock block (mutex for job q)
        {
            std::unique_lock<std::mutex> lock(job_mutex);
            
            //wait for job q to have a job or shutdown
            job_ready.wait(lock, [] { return !job_q.empty() || !server_running; });
            //check if shutdown case broke the wait (aka !server_running) and break
            if (!server_running) {
                break;
            } else {
                //store job in from job Q
                job = job_q.front();
                //remove job in from job Q
                job_q.pop();
            }
        }

        /////////////////////MESSAGE HANDLING/////////////////
        //get the message to handle
        std::vector<uint8_t>& client_request = job.client_message;
        std::uint8_t& decision_byte = client_request.front();
        //create response (change default to failure)
        std::vector<uint8_t> response;
        //lock db mutex (delete if read/write leader election happens)
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            switch (decision_byte) {
                case INDEX_REQUEST: {
                    FileId file_id = parseIndexRequest(client_request);
                    if (EXIT_SUCCESS != db->indexFile(file_id.uuid, 
                                                      file_id.indexer, 
                                                      file_id.f_size))
                        response = createFailMessage(db->sqliteError()); 
                    else
                        response = {INDEX_OK}; //ack-byte
                    break;
                }

                case DROP_REQUEST: {
                    //see messageFormatting for IndexUuidPair
                    IndexUuidPair uuids = parseDropRequest(client_request);
                    if (EXIT_SUCCESS != db->dropIndex(uuids.first, uuids.second))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {DROP_OK};
                    break;
                }

                case REREGISTER_REQUEST: {
                    SourceInfo client_info = parseReregisterRequest(client_request);
                    if (EXIT_SUCCESS != db->updateClient(client_info))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = {REREGISTER_OK};
                    break;   
                }

                case SOURCE_REQUEST: {
                    uint64_t f_uuid = parseSourceRequest(client_request);
                    std::vector<SourceInfo> indexers;
                    if (EXIT_SUCCESS != db->grabSources(f_uuid, indexers))
                        response = createFailMessage(db->sqliteError());
                    else
                        response = createSourceList(indexers);
                    break;
                }

                case SERVER_REG: {
                    SourceInfo new_server = parseNewServerReg(client_request);
                    ssize_t registered_with = forwardRegistration(client_request, known_servers);
                    //NEEDS CHECKS FOR DEAD SERVERS HERE TODO
                    if (registered_with < 0) {
                        response = createFailMessage("I appear to be a dead server myself?");
                    } else {
                        response = createServerRegResponse(known_servers);
                        known_servers.push_back(new_server);
                    }
                    break;
                }

                case FORWARD_SERVER_REG: {
                    SourceInfo new_server = parseForwardServerReg(client_request);
                    known_servers.push_back(new_server);
                    response = {FORWARD_SERVER_OK};
                    break;
                }

                case CLIENT_REG: {
                    response = createServerRegResponse(known_servers);
                    break;
                }

                default:
                    response = createFailMessage("Invalid message type.");
            }
        }
        
        std::cout << "[DEBUG] SERVERS:" << std::endl;
        for (auto& s : known_servers) {
            std::cout << s.ip_addr << " " << s.port << std::endl;
        }


        //send response to client
        tcp::sendMessage(job.client_sock, response);

        //close the jobs socket (with client file directory)
        closeSocket(job.client_sock);
    }
}

void listenThread(const uint16_t port) {
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
            std::thread(handleConnectionThread, client_sock).detach();
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
    db = new Database("name.db");

    //output msg
    std::cout << "DB setup complete\n";

    //start the listen thread
    //all new connections are received here, and handed off for processing
    std::thread listener_thread(listenThread, port);

    //vector of threads containing our workers
    std::vector<std::thread> workers;
    
    //create worker threads (currently 4 as example did, but number has no meaning)
    for (int i = 0; i < 4; ++i) {
        workers.push_back(std::thread(workerThread));
    }

    //start setup thread with known server *only if a known server was provided*
    std::thread setup_thread;
    if (!known_server.ip_addr.empty()) {
        setup_thread = std::thread(setupThread, known_server);
        setup_thread.join();
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
