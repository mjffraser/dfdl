#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mutex>

#include "client/internal/peerRequests.hpp"
#include "client/internal/internal/clientNetworking.hpp"
#include "networking/socket.hpp"
#include "networking/messageFormatting.hpp"

namespace dfd
{
    void seedToPeer(std::atomic<bool> &shutdown, int peer_sock)
    {
    }

    void clientListener(std::atomic<bool> &shutdown, std::atomic<uint16_t> &port)
    {
        // open a listener
        auto sock_port = openSocket(true, 0);
        int my_listen_sock;
        if (!sock_port)
        {
            std::cerr << "[clientListener] Could not create and bind socket to index to peers.\n";
            return;
        }

        my_listen_sock = sock_port->first;
        port.store(sock_port->second);

        std::cout << "[clientListener] Listening on port " << port.load() << std::endl;

        // start listening for incoming clients
        if (listen(my_listen_sock, MAX_PEER_THREADS))
        {
            std::cerr << "[clientListener] Could not start listening.\n";
            closeSocket(my_listen_sock);
            return;
        }

        std::vector<std::thread> seed_threads;

        while (!shutdown.load()) // Keep listening until shutdown is requested
        {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int peer_sock = accept(my_listen_sock, (sockaddr *)&client_addr, &client_len);
            if (peer_sock < 0)
            {
                if (shutdown.load())
                    break; // Exit if we're shutting down
                std::cerr << "[clientListener] Error accepting connection.\n";
                continue; // Try to accept the next connection
            }

            // Launch a thread to handle the incoming peer connection
            seed_threads.emplace_back(seedToPeer, std::ref(shutdown), peer_sock);
        }

        // Wait for all threads to finish
        for (auto &st : seed_threads)
        {
            if (st.joinable())
                st.join();
        }

        closeSocket(my_listen_sock); // Close the listening socket when done
    }
}