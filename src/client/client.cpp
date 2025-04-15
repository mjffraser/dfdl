#include "client/client.hpp"
#include "client/internal/clientConfigs.hpp"
#include "client/internal/requests.hpp"
#include "client/internal/clientThreads.hpp"
#include "networking/fileParsing.hpp"
#include "sourceInfo.hpp"

#include <csignal>
#include <map>
#include <thread>
#include <unistd.h>
#include <variant>
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

enum message_code {
    EXIT,
    LIST,
    HELP,
    INDEX,
    DROP,
    DOWNLOAD,
    CRASH,
};

//maps user supplied arg to container, converting to uuid if needed
template <typename T>
int getArg(const std::string& command, T& arg_container) {
    std::vector<std::string> args;
    std::string curr_str;
    for (const char& c : command) {
        if (c == ' ' && !curr_str.empty()) {
            args.push_back(curr_str);
            curr_str.clear();
        } else {
            curr_str.push_back(c);
        }
    } if (!curr_str.empty()) args.push_back(curr_str);

    if (args.size() != 2) 
        return EXIT_FAILURE;
    const std::string& ret_arg = args[1];
    if constexpr (std::is_same_v<T, std::string>) {
        arg_container = ret_arg;
        return EXIT_SUCCESS;
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        try {
            arg_container = std::stoull(ret_arg);
            return EXIT_SUCCESS;
        } catch (...) {
            std::cerr << "[err] Invalid UUID: Please provide a numeric UUID!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    //otherwise
    return EXIT_FAILURE;
}

std::optional<message_code> parseCommand(const std::string&                   command,
                                         std::variant<uint64_t, std::string>& command_arg) {
    //no arg commands
    if (command.substr(0,5) == "exit "  || command == "exit")  return EXIT;
    if (command.substr(0,5) == "list "  || command == "list")  return LIST;
    if (command.substr(0,5) == "help "  || command == "help")  return HELP;
    if (command.substr(0,6) == "crash " || command == "crash") return CRASH;

    //index command, takes std::string as arg
    if (command.substr(0,5) == "index") {
        std::string f_path;
        if (EXIT_FAILURE == getArg(command, f_path)) {
            std::cerr << "[err] Invalid command: " << command << std::endl;
            std::cerr << "[err] Usage: index <path to file>"  << std::endl;
            return std::nullopt;
        }

        command_arg = f_path;
        return INDEX;
    }

    //drop command, takes std::string as arg
    if (command.substr(0,4) == "drop") {
        std::string f_path;
        if (EXIT_FAILURE == getArg(command, f_path)) {
            std::cerr << "[err] Invalid command: " << command << std::endl;
            std::cerr << "[err] Usage: drop <path to file>"   << std::endl;
            return std::nullopt;
        }

        command_arg = f_path;
        return DROP;
    }

    if (command.substr(0,8) == "download") {
        uint64_t uuid;
        if (EXIT_FAILURE == getArg(command, uuid)) {
            std::cerr << "[err] Invalid command: " << command << std::endl;
            std::cerr << "[err] Usage: download <uuid>"       << std::endl;
            return std::nullopt;
        }

        command_arg = uuid;
        return DOWNLOAD;
    }

    return std::nullopt;
}

void client_main(const std::string&                     listen_addr,
                 const uint16_t                         listener_port,
                       std::map<uint64_t, std::string>& indexed_files,
                       std::mutex&                      indexed_files_mtx) {
    //my identity for the server to use
    SourceInfo my_listener;
    my_listener.ip_addr = listen_addr;
    my_listener.port    = listener_port;
    my_listener.peer_id = my_uuid;

    //handle CONTROL+C
    signal(SIGINT, signalHandler);

    //welcome messages
    std::cout << "Welcome to P2P Client!"                                  << std::endl;
    std::cout << "Your current download directory is: " << my_download_dir << std::endl;
    std::cout << "Type 'help' for commands."                               << std::endl;


    //main loop
    while (!shutdown) {
        std::string                         command;
        std::variant<uint64_t, std::string> command_arg;

        //get user input
        std::cout << "> ";
        if (!std::getline(std::cin, command)) break;

        auto code = parseCommand(command, command_arg);
        if (!code) {
            std::cerr << "Unknown command. Type 'help' for usage." << std::endl;
            continue;
        }

        //execute command
        switch (code.value()) {
            case EXIT: {
                shutdown = true;
                break;
            }

            case LIST: {
                // handleList
                break;
            }

            case HELP: {
                printHelp();
                break;
            }

            case INDEX: {
                doIndex(my_listener,
                        std::get<std::string>(command_arg), //file name
                        indexed_files,
                        indexed_files_mtx,
                        server_list);
                break;
            }

            case DROP: {
                doDrop(my_listener,
                       std::get<std::string>(command_arg), //file name
                       indexed_files,
                       indexed_files_mtx,
                       server_list);
                break;
            }

            case DOWNLOAD: {
                doDownload(std::get<uint64_t>(command_arg), //file uuid
                           server_list);
                break;
            }

            case CRASH: {
                exit(-1);
            }
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

    //container and mutex for all indexed files
    std::map<uint64_t, std::string> indexed_files;
    std::mutex                      indexed_files_mtx;

    //listener setup
    std::atomic<uint16_t> listener_port  = 0;
    std::atomic<bool>     listener_setup = false;
    std::thread           my_listener    = std::thread(clientListener,
                                                       std::ref(shutdown),
                                                       std::ref(listener_port),
                                                       std::ref(listener_setup),
                                                       std::cref(indexed_files),
                                                       std::ref(indexed_files_mtx));

    //wait while listener sets up
    sleep(1);
    if (!listener_setup) {
        my_listener.join();
        std::cerr << "Could not finish startup. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }

    client_main(listen_addr, listener_port, indexed_files, indexed_files_mtx);

    //shutdown
    std::cout << "Shutting down..." << std::endl;
    shutdown = true;

    storeHostListToDisk(server_list, HOST_FILE_NAME);

    my_listener.join(); 
    exit(EXIT_SUCCESS);
}

} //dfd
