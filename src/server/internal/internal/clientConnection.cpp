#include "server/internal/internal/clientConnection.hpp"
#include "config.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include <atomic>
#include <condition_variable>
#include <thread>
#include <array>
#include <iostream>
#include "server/internal/syncing.hpp"
#include "server/internal/serverStartup.hpp"

namespace dfd {

//sends a message to the client every 1s to stop timeouts
//open as thread, notify with cv
void keepClientAlive(int client_fd,
                     std::atomic<bool>& run,
                     std::mutex& m,
                     std::condition_variable& cv) {
    while (run) {
        std::cout << "sent." << std::endl;
        tcp::sendMessage(client_fd, {KEEP_ALIVE});
        std::unique_lock<std::mutex> lock(m);
        if (cv.wait_for(lock, std::chrono::milliseconds(1000), [&run] {return !run;}))
            break;
    }
    return;
}


//receives tcp message with timeout. closes socket if nothing received.
int recvClientRequest(int client_sock, std::vector<uint8_t>& buff) {
    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;
    ssize_t bytes_read = tcp::recvMessage(client_sock, buff, timeout);
    if (bytes_read <= 0) {
        closeSocket(client_sock);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

//receives udp message with timeout. DOES NOT close socket if nothing received
int recvWorkerMessage(int worker_sock, SourceInfo& src_info, std::vector<uint8_t>& buff) {
    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 500000;
    return udp::recvMessage(worker_sock, src_info, buff, timeout);
}

std::pair<int, uint16_t> selectWorker(std::vector<uint8_t>&                            client_request,
                                      std::atomic<int>&                                next_reader,
                                      std::array<std::atomic<bool>, WORKER_THREADS  >& worker_stats,
                                      std::array<uint16_t,          WORKER_THREADS-1>& read_workers,
                                      uint16_t&                                        write_worker) {
    int      worker_id;
    uint16_t worker_port;
    if (*client_request.begin() == SOURCE_REQUEST) {
        //READ REQUEST, FIND A READ THREAD
        worker_id = next_reader;
        next_reader = (next_reader+1) % (WORKER_THREADS-1);
        if (!worker_stats[worker_id] || read_workers[worker_id] == 0)
            worker_id = -1;
        else
            worker_port = read_workers[worker_id];
    } else {
        //WRITE REQUEST, USE LEADER
        worker_id   = WORKER_THREADS-1; //this is always the case
        worker_port = write_worker; 
    }
    
    std::cout << "WORKER: " << worker_id << std::endl;
    return std::make_pair(worker_id, worker_port);
}

void broadcastToServers(std::vector<uint8_t>&      client_request,
                        std::vector<SourceInfo>&   known_servers,
                        std::mutex&                known_server_mtx) {
    std::cout << "BROADCASTING" << std::endl;
    std::vector<uint8_t> req_copy(client_request);
    switch (*req_copy.begin()) {
        //NOTE: added breaks to all cases cause was not sure if needed
        case INDEX_REQUEST: {
            //use
            auto failed_servers = forwardIndexRequest(req_copy, known_servers);
            if (!failed_servers.empty()){
                removeFailedServers(known_servers, failed_servers, known_server_mtx);
            }
            break;
        }

        case DROP_REQUEST: {
            //use
            auto failed_servers = forwardDropRequest(req_copy, known_servers);
            if (!failed_servers.empty()){
                removeFailedServers(known_servers, failed_servers, known_server_mtx);
            }
            break;
        }

        case REREGISTER_REQUEST: {
            //use
            auto failed_servers = forwardReregRequest(req_copy, known_servers);
            if (!failed_servers.empty()){
                removeFailedServers(known_servers, failed_servers, known_server_mtx);
            }
            break;
        }

        //SYNCING STUFF
        case SERVER_REG: {
            SourceInfo new_server = parseNewServerReg(req_copy);
            //server reg
            ssize_t registered_with = forwardRegistration(req_copy, known_servers);
            std::cout << "number of servers registration was forwarded and acked" << registered_with << std::endl;
            break;
        }
                
        default: {}
    }
}

void workerNoReply(int                                              udp_sock,
                   int                                              worker_id,
                   std::atomic<int>&                                next_reader,
                   std::array<std::thread,       WORKER_THREADS  >& workers,
                   std::array<std::atomic<bool>, WORKER_THREADS  >& worker_stats,
                   std::array<std::atomic<int>,  WORKER_THREADS  >& worker_strikes,
                   std::array<uint16_t,          WORKER_THREADS-1>& read_workers,
                   uint16_t&                                        write_worker,
                   std::mutex&                                      election_mtx) {
    worker_strikes[worker_id]++;
    if (worker_strikes[worker_id] < 5)
        return; //not enough strikes yet

    //read thread is down
    if (worker_id != WORKER_THREADS-1) {
        worker_stats[worker_id]   = false;
        worker_strikes[worker_id] = 0;
    }

    //write thread is down
    else {
        std::unique_lock<std::mutex> lock(election_mtx, std::try_to_lock);
        if (!lock.owns_lock())
            return; //another thread is already calling an election
        worker_stats[worker_id] = false;
        
        std::vector<uint8_t> election_msg = {ELECT_LEADER};
        SourceInfo reader_addr;
        reader_addr.ip_addr = "127.0.0.1"; reader_addr.port = read_workers[next_reader];

        //send election message
        udp::sendMessage(udp_sock, reader_addr, election_msg);

        //get response
        std::vector<uint8_t> response;
        recvWorkerMessage(udp_sock, reader_addr, response);
        if (response.size() > 0 && *response.begin() == LEADER_X) {
            int leader_ind = (int)response[1];
            std::cout << "ELECTED LEADER=" << leader_ind << std::endl;

            //set leader
            write_worker              = read_workers[leader_ind];
            worker_stats[leader_ind]  = false;
            read_workers[leader_ind]  = 0;
            worker_strikes[worker_id] = 0;
            worker_stats[worker_id]   = true;

            std::swap(workers[leader_ind], workers[WORKER_THREADS-1]);
        }
    }
}

struct KeepAliveThread {
    std::atomic<bool>       still_running = true;
    std::mutex              keep_alive_mtx;
    std::condition_variable keep_alive_cv;
    std::thread             keep_alive_thread;

    KeepAliveThread(int client_sock) {
        keep_alive_thread = std::thread(keepClientAlive,
                                        client_sock,
                                        std::ref(still_running),
                                        std::ref(keep_alive_mtx),
                                        std::ref(keep_alive_cv));
    }

    ~KeepAliveThread() {
        {
            std::lock_guard<std::mutex> lock(keep_alive_mtx);
            still_running = false;
        }
        keep_alive_cv.notify_one();
        keep_alive_thread.join();
    }
};

void clientConnection(int                                              client_sock,
                      std::atomic<int>&                                next_reader,
                      std::array<std::thread,       WORKER_THREADS  >& workers,
                      std::array<std::atomic<bool>, WORKER_THREADS  >& worker_stats,
                      std::array<std::atomic<int>,  WORKER_THREADS  >& worker_strikes,
                      std::array<uint16_t,          WORKER_THREADS-1>& read_workers,
                      uint16_t&                                        write_worker,
                      std::mutex&                                      election_mtx,
                      std::vector<SourceInfo>&                         known_servers,
                      std::mutex&                                      known_server_mtx) {
    //receive client message
    std::vector<uint8_t> client_request;
    SourceInfo client;
    if (EXIT_FAILURE == recvClientRequest(client_sock, client_request))
        return;

    if (*client_request.begin() == SERVER_REG) {
        SourceInfo si = parseNewServerReg(client_request);
        client.ip_addr = si.ip_addr;
        client.port    = si.port;
    }

    if (*client_request.begin() == DOWNLOAD_INIT) {
        databaseSendNS(client_sock);

        closeSocket(client_sock);
        return;
    }
    
    //open udp sock to talk to worker
    auto udp_sock_init = openSocket(false, 0, true);
    if (!udp_sock_init) {
        closeSocket(client_sock);
        return;
    }

    //start up keep alive thread
    KeepAliveThread* keep_alive = new KeepAliveThread(client_sock); 

    int worker_sock = udp_sock_init.value().first;
    for (int i = 0; i < 10; ++i) {
        //get address of worker to contact
        SourceInfo worker_addr; worker_addr.ip_addr = "127.0.0.1"; //workers are internal
        auto res = selectWorker(client_request, next_reader, worker_stats, read_workers, write_worker);
        if (res.first == -1) continue; //read thread is down, we skip it
        int worker_id    = res.first;
        worker_addr.port = res.second;

        //send request 
        if (EXIT_FAILURE == udp::sendMessage(worker_sock, worker_addr, client_request)) {
            worker_strikes[worker_id]++;
            continue;
        }

        //get response
        std::vector<uint8_t> worker_response;
        if (EXIT_SUCCESS == recvWorkerMessage(worker_sock, worker_addr, worker_response)) {
            std::cout << "SENT CODE:"   << (int)*client_request.begin() << std::endl;
            if ((int)*worker_response.begin() == 0)
                std::cout << "[ERR] " << parseFailMessage(worker_response) << std::endl;
            
            //WE GOT A REPLY
            worker_strikes[worker_id] = 0;
            
            //shutdown keep alive thread
            delete keep_alive;

            //reply to client
            tcp::sendMessage(client_sock, worker_response);
            closeSocket(client_sock);

            //sync with other servers
            broadcastToServers(client_request, known_servers, known_server_mtx);

            //if server reg, add the server to my list
            if (*client_request.begin() == SERVER_REG) {
                std::lock_guard<std::mutex> lock(known_server_mtx);
                known_servers.push_back(client);
            }

            std::cout << "SERVER LIST:" << std::endl;
            for (auto& serv : known_servers) {
                std::cout << serv.ip_addr << " " << serv.port << std::endl;
            }

            return;
        }
        
        //WE DIDN'T GET A REPLY FAST ENOUGH
        workerNoReply(worker_sock,
                      worker_id,
                      next_reader,
                      workers,
                      worker_stats,
                      worker_strikes,
                      read_workers,
                      write_worker,
                      election_mtx);
    }

    delete keep_alive;
    auto fail_msg = createFailMessage("Database appears to be down. Sorry, please try another server.");
    tcp::sendMessage(client_sock, fail_msg);
    closeSocket(client_sock);

}

}
