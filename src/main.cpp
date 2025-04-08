#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "client/client.hpp"
#include "server/server.hpp"

//client & server
static uint16_t port = 0;

//client-specific
static std::string download_dir = "";
static uint64_t    my_uuid      = 0;
static std::string ip_addr = "";
static std::string listen_addr;

//server-specific
static std::string connect_ip;
static uint16_t    connect_port;

bool isServer(int argc, char **argv) {
    bool is_server = false;
    bool is_client = false;
    for (int i = 1; i < argc; i++) {
        //shared options
        try {
            if (std::string(argv[i]) == "--port")
                if (i+1 < argc) port = std::stoi(argv[i+1]);
        } catch (...) {
            std::cerr << "USAGE: --port <#>" << std::endl;
            exit(-1);
        }

        //server options
        if (std::string(argv[i]) == "--server") {
            is_server = true;
        }

        if (std::string(argv[i]) == "--connect") {
            try {
                if (i+2 < argc) {
                    connect_ip   = argv[i+1];
                    connect_port = std::stoi(argv[i+2]);
                    if (connect_ip == "localhost")
                        connect_ip = "127.0.0.1";
                } else {
                    throw std::runtime_error("");
                }
            } catch (...) {
                std::cerr << "USAGE: --connect <IPv4 ADDR> <port #>" << std::endl;
                exit(-1);
            }
        }

        //client options
        if (std::string(argv[i]) == "--client") {
            is_client = true;
        }

        //server to connect to
        if (std::string(argv[i]) == "--ip") {
            if (i+1 < argc)             ip_addr = argv[i+1];
            if (ip_addr == "localhost") ip_addr = "127.0.0.1";
        }

        //download dir
        if (std::string(argv[i]) == "--download") {
            if (i+1 < argc)
                download_dir = argv[i+1];
        }

        //listen addr
        if (std::string(argv[i]) == "--listen") {
            if (i+1 < argc)                 listen_addr = argv[i+1];
            if (listen_addr == "localhost") listen_addr = "127.0.0.1";
        }
    }

    //check what we got
    if (is_server && is_client) {
        std::cerr << "Cannot be both client and server!" << std::endl;
        exit(-1);
    }

    if (!is_server && !is_client) {
        std::cerr << "Must specify either --server OR --client" << std::endl;
        exit(-1);
    }

    //server needs port to open on, client needs server port to connect to
    if (port == 0 && is_server) {
        std::cerr << "Missing port!" << std::endl;
        exit(-1);
    }

    if (is_server && !(port < 65535 && port > 1023)) {
        std::cerr << "Port must be between 1024 & 65535 inclusive." << std::endl;
        exit(-1);
    }

    if (is_client) {
        if (listen_addr.length() == 0) {
            std::cerr << "Missing listen IP!" << std::endl;
            exit(-1);
        }

        if (port != 0 && !(port < 65535 && port > 1023)){
            std::cerr << "Port must be between 1024 & 65535 inclusive." << std::endl;
            exit(-1);
        }
    }

    return is_server;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * main
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Main distributed file downloading software.
 *
 * Takes:
 * -> argc:
 *    Number of command line args supplied.
 *
 * -> argv:
 *    The array of args.
 *
 * Returns:
 * -> On success:
 *    0
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char** argv) {
    bool server = isServer(argc, argv); //doubles as arg parsing
    if (server) {
        dfd::run_server(port, connect_ip, connect_port);
        return 0;
    }

    //else client
    dfd::run_client(ip_addr, port, download_dir, listen_addr);
    return 0;
}
