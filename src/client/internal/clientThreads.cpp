#include <atomic>
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>

#include "client/internal/clientThreads.hpp"
#include "client/internal/internal/seedThread.hpp"
#include "networking/socket.hpp"
#include "sourceInfo.hpp"

namespace dfd {
    

void clientListener(std::atomic<bool>&     shutdown,
                    std::atomic<uint16_t>& listener_port,
                    std::atomic<bool>&     listener_setup) {
    // open a listener
    auto sock_port = openSocket(true, 0);
    int my_listen_sock;
    if (!sock_port) {
        std::cerr << "[clientListener] Could not create and bind socket to index to peers.\n";
        return;
    }

    my_listen_sock = sock_port->first;
    listener_port.store(sock_port->second);

    // std::cout << "[clientListener] Listening on port " << sock_port->second << std::endl;

    // start listening for incoming clients
    if (tcp::listen(my_listen_sock, MAX_PEER_THREADS)) {
        std::cerr << "[clientListener] Could not start listening.\n";
        closeSocket(my_listen_sock);
        return;
    }

    listener_setup = true;
    std::vector<std::thread> seed_threads;
    struct timeval accept_timeout;
    accept_timeout.tv_sec  = 1;
    accept_timeout.tv_usec = 0;
    while (!shutdown.load()) { //Keep listening until shutdown is requested
        SourceInfo client;
        int peer_sock = tcp::accept(my_listen_sock, client, accept_timeout);
        if (peer_sock < 0) {
            if (shutdown.load())
                break; // Exit if we're shutting down
            // std::cerr << "[clientListener] Error accepting connection.\n";
            continue; // Try to accept the next connection
        }

        // Launch a thread to handle the incoming peer connection
        // TODO FIX MEMORY LEAK
        seed_threads.emplace_back(seedToPeer, std::ref(shutdown), peer_sock);
    }

    // Wait for all threads to finish
    for (auto &st : seed_threads) {
        if (st.joinable())
            st.join();
    }

    closeSocket(my_listen_sock); // Close the listening socket when done
}

} //dfd
