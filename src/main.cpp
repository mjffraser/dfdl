#include <cstdint>
#include <iostream>

#include "client/client.hpp"

static std::string ip_addr;
static uint16_t    port;

bool isServer(int argc, char **argv) {
    bool is_server = false;
    bool is_client = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--server") is_server = true;
        if (std::string(argv[i]) == "--client") is_client = true;
        if (std::string(argv[i]) == "--ip")     ip_addr   = argv[i];
        if (std::string(argv[i]) == "--port")   port      = std::stoi(argv[i]);
    }

    if (is_server && is_client) {
        std::cerr << "Cannot be both client and server!" << std::endl;
        exit(-1);
    }

    if (!is_server && !is_client) {
        std::cerr << "Must specify either --server OR --client" << std::endl;
        exit(-1);
    }

    if (is_server) {
        if (ip_addr.length() == 0) {
            std::cerr << "Missing IP!" << std::endl;
            exit(-1);
        }
        if (!(port < 65535 && port > 1023)) {
            std::cerr << "Missing Port!" << std::endl;
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
 * -> Entry point for the P2P Client app. Instantiates P2PClient and runs it.
 *
 * Takes:
 * -> argc, argv: Standard command-line arguments.
 *
 * Returns:
 * -> 0 on success.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int main(int argc, char** argv) {
    bool server = isServer(argc, argv);
    if (server) {
        //START SERVER FUNCTION 
        return 0;
    }

    //else client
    dfd::P2PClient client(ip_addr, port);
    client.run();
    return 0;
}
