#include "server/internal/internal/electionThread.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"

#include <array>
#include <thread>

namespace dfd {

void electionThread(std::atomic<bool>&                             server_running,
                    int                                            thread_ind,
                    std::pair<int, uint16_t>                       my_addr,
                    std::atomic<bool>&                             call_election,
                    uint16_t&                                      requester_port,
                    std::array<std::atomic<bool>, WORKER_THREADS>& worker_stats,
                    std::array<uint16_t, WORKER_THREADS-1>&        election_listeners,
                    std::atomic<int>&                              setup_workers,
                    std::atomic<int>&                              setup_election_workers) {
    int listener = my_addr.first;
    election_listeners[thread_ind] = my_addr.second;

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
                    //ELECT LEADER HERE
                    in_election = false;
                    SourceInfo asker; asker.ip_addr = "127.0.0.1"; asker.port = requester_port;
                    udp::sendMessage(listener, asker, {LEADER_X, (uint8_t)thread_ind});
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

}
