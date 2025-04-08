#include "client/client.hpp"
#include "client/internal/clientConfigs.hpp"
#include "client/internal/requests.hpp"
#include "client/internal/clientThreads.hpp"
#include "networking/fileParsing.hpp"
#include "sourceInfo.hpp"

#include <csignal>
#include <thread>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <atomic>

namespace dfd {

inline static const std::string HOST_FILE_NAME = "hosts";
inline static const std::string UUID_FILE_NAME = "uuid";

std::vector<SourceInfo>  server_list;
uint64_t                 my_uuid = 0;
std::string              my_download_dir;

int client_startup(const std::string& ip, 
                   const uint16_t     port,
                   const std::string& download_dir) {
    //load at minimum one server
    getHostListFromDisk(server_list, HOST_FILE_NAME);
    if (!ip.empty() && port != 0) {
        SourceInfo cmdline_addr;
        cmdline_addr.ip_addr = ip;
        cmdline_addr.port    = port;
        server_list.push_back(cmdline_addr);
    } 

    //check we have at minimum one server
    if (server_list.empty()) {
        std::cerr << "No host file or IP:port was provided. Who do I connect to?" << std::endl;
        return EXIT_FAILURE;
    }

    //load uuid & check
    my_uuid = getMyUUID(UUID_FILE_NAME);
    if (my_uuid == 0) {
        std::cerr << "Could not initialize a uuid. Is this a linux machine?" << std::endl;
        return EXIT_FAILURE; 
    }

    if (!download_dir.empty()) {
        setDownloadDir(download_dir);
        my_download_dir = download_dir;
    } else {
        my_download_dir = initDownloadDir();
    }

    //check we have a download dir
    if (my_download_dir.empty()) {
        std::cerr << "Could not initialize the download directory. Do you have read/write permission in your home directory/specified path?" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static std::atomic<bool> shutdown = false;

void signalHandler(int sig) {
    shutdown = true;
}

void printHelp() {
    std::cout << "Available commands:\n";
    std::cout << "  list                - List all currently indexed files\n";
    std::cout << "  index <filename>    - Register/share <filename>\n";
    std::cout << "  download <filename> - Download <filename> from a peer\n";
    std::cout << "  drop <filename>     - Remove <filename> from the server\n";
    std::cout << "  help                - Show this message\n";
    std::cout << "  exit                - Quit the client\n";
}

void client_main(const std::string& listen_addr, const uint16_t listener_port) {
    //my identity for the server to use
    SourceInfo my_listener;
    my_listener.ip_addr = listen_addr;
    my_listener.port    = listener_port;
    my_listener.peer_id = my_uuid;

    std::vector<std::string> indexed_files;

    std::string command;
    signal(SIGINT, signalHandler);
    std::cout << "Welcome to P2P Client!\n";
    std::cout << "Your current download directory is: " << my_download_dir << std::endl;
    std::cout << "Type 'help' for commands.\n";

    while (!shutdown) {
        std::cout << "> ";
        if (!std::getline(std::cin, command)) break;
        
        if (command == "exit") {
            shutdown = true;
        } 

        else if (command == "list") {
            // handleList();
        } 

        else if (command.rfind("index ", 0) == 0) {
            // e.g. "index myfile.txt"
            std::string file_name = command.substr(6);
            doIndex(my_listener, file_name, indexed_files, server_list);
        } 

        else if (command.rfind("download ", 0) == 0) {
            // e.g. "download <file uuid>"
            std::string uuid_str = command.substr(9);
            try {
                uint64_t file_uuid = std::stoull(uuid_str);
                // handleDownload(file_uuid);
            } catch (const std::exception& e) {
                std::cerr << "Invalid file UUID format. Please provide a valid numeric ID.\n";
            }
        } 

        else if (command.rfind("drop ", 0) == 0) {
            // e.g. "remove <file uuid>"
            std::string file_name = command.substr(5);
            doDrop(my_listener, file_name, indexed_files, server_list);
        } 

        else if (command == "help") {
            printHelp();
        } 

        else if (command == "crash") {
            exit(-1);
        } 

        else {
            std::cout << "Unknown command. Type 'help' for usage.\n";
        }
    }

    //if a signal killed the main loop
    if (!shutdown) shutdown = true;
}

void run_client(const std::string& ip,
                const uint16_t     port,
                const std::string& download_dir,
                const std::string& listen_addr) {
    //setup and input validation
    if (EXIT_FAILURE == client_startup(ip, port, download_dir))
        exit(EXIT_FAILURE);
    std::cout << "Setup with " << server_list.size() << " servers." << std::endl;

    //listener setup
    std::atomic<uint16_t> listener_port  = 0;
    std::atomic<bool>     listener_setup = false;
    std::thread           my_listener    = std::thread(clientListener,
                                                       std::ref(shutdown),
                                                       std::ref(listener_port),
                                                       std::ref(listener_setup));

    //wait while listener sets up
    sleep(1);
    if (!listener_setup) {
        my_listener.join();
        std::cerr << "Could not finish startup. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }

    client_main(listen_addr, listener_port);

    //shutdown
    std::cout << "Shutting down..." << std::endl;
    shutdown = true;
    my_listener.join(); 
    exit(EXIT_SUCCESS);
}

} //dfd
