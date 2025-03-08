#include <iostream>

bool isServer(int argc, char **argv) {
    bool is_server = false;
    bool is_client = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--server") is_server = true;
        if (std::string(argv[i]) == "--client") is_client = true;
    }

    if (is_server && is_client) {
        std::cerr << "Cannot be both client and server!" << std::endl;
        exit(-1);
    }

    if (!is_server && !is_client) {
        std::cerr << "Must specify either --server OR --client";
        exit(-1);
    }

    return is_server;
}

int main(int argc, char** argv) {
    bool server = isServer(argc, argv);
    if (server) {
        //START SERVER FUNCTION 
    }

    //else client
    //START CLIENT FUNCTION
}
