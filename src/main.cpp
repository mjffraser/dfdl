#include <cstdint>
#include <iostream>
#include <unistd.h>

#include "client/client.hpp"
#include "server/server.hpp"

static std::string ip_addr;
static std::string download_dir = "";
static uint16_t    port         = 0;
static uint64_t    my_uuid      = 0;

bool isServer(int argc, char **argv) {
    bool is_server = false;
    bool is_client = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--server") is_server = true;
        if (std::string(argv[i]) == "--client") is_client = true;
        if (std::string(argv[i]) == "--ip") 
            if (i+1 < argc)
                ip_addr = argv[i+1];
        if (std::string(argv[i]) == "--port") 
            if (i+1 < argc) 
                port = std::stoi(argv[i+1]);
        if (std::string(argv[i]) == "--uuid") 
            if (i+1 < argc) 
                my_uuid = std::stoull(argv[i+1]);
        if (std::string(argv[i]) == "--download")
            if (i+1 < argc)
                download_dir = argv[i+1];
    }

    if (is_server && is_client) {
        std::cerr << "Cannot be both client and server!" << std::endl;
        exit(-1);
    }

    if (!is_server && !is_client) {
        std::cerr << "Must specify either --server OR --client" << std::endl;
        exit(-1);
    }

    //server needs port to open on, client needs server port to connect to
    if (!(port < 65535 && port > 1023)) {
        std::cerr << "Missing Port!" << std::endl;
        exit(-1);
    }

    if (is_client) {
        //client needs a server ip
        if (ip_addr.length() == 0) {
            std::cerr << "Missing IP!" << std::endl;
            exit(-1);
        }
        
        //client needs a uuid TODO: move to file?
        if (my_uuid == 0) {
            std::cerr << "Please supply a uuid!" << std::endl;
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
        dfd::mainServer(port);
        return 0;
    }

    //else client
    dfd::P2PClient client(ip_addr, port, my_uuid, download_dir);
    client.run();
    return 0;
}



