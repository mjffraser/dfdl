
#include "client/client.hpp"
#include "networking/messageFormatting.hpp"
#include "sourceInfo.hpp"
#include "networking/socket.hpp"
#include "networking/fileParsing.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <optional>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <thread>
#include <mutex>
#include <filesystem>

namespace dfd
{

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
P2PClient::P2PClient(const std::string& server_ip, int server_port)
  : server_ip_(server_ip),
    server_port_(server_port),
    is_running_(true),
    listen_sock_(-1)
{
    // We could attempt a test connection to the server here or do a lazy connect
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
P2PClient::~P2PClient()
{
    stopAllSharing();
    is_running_ = false;
}

//------------------------------------------------------------------------------
// Public: main command loop
//------------------------------------------------------------------------------
void P2PClient::run()
{
    std::string command;
    std::cout << "Welcome to P2P Client!\n";
    std::cout << "Type 'help' for commands.\n";

    while (is_running_) {
        std::cout << "> ";
        if (!std::getline(std::cin, command)) {
            break;
        }

        if (command == "exit") {
            std::cout << "Exiting...\n";
            is_running_ = false;
        } else if (command.rfind("index ", 0) == 0) {
            // e.g. "index myfile.txt"
            std::string file_name = command.substr(6);
            handleIndex(file_name);
        } else if (command.rfind("download ", 0) == 0) {
            // e.g. "download <file uuid>"
            uint64_t file_uuid = std::stoull(command.substr(9));
            handleDownload(file_uuid);
        // } else if (command.rfind("remove ", 0) == 0) {
        //     // e.g. "remove <file uuid>"
        //     std::string file_name = command.substr(7);
        //     handleRemove(file_name);
        } else if (command == "help") {
            printHelp();
        } else {
            std::cout << "Unknown command. Type 'help' for usage.\n";
        }
    }
}

//------------------------------------------------------------------------------
// Private: handle "index <file>"
//------------------------------------------------------------------------------
void P2PClient::handleIndex(const std::string& file_name)
{

    std::filesystem::path f_path = std::filesystem::path("storage") / file_name;


    uint64_t file_id = sha512Hash(f_path);
    if(file_id == 0) {
        std::cerr << "Failed to compute file_id for '" << file_name << "'.\n";
        return;
    }


    std::optional<ssize_t> file_size;
    try {
        file_size = fileSize(f_path);
    } catch(const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to get file size for '"
                  << f_path << "': " << e.what() << std::endl;
        return;
    }

    if (!file_size.has_value()) {
        std::cerr << "Failed to retrieve file size for: " << f_path << "\n";
        return;
    }

    uint64_t uint_file_size = static_cast<uint64_t>(file_size.value());


    SourceInfo my_source;
    my_source.peer_id = 123456;
    my_source.ip_addr = getLocalIPAddress();
    my_source.port    = static_cast<uint16_t>(getListeningPort());


    dfd::FileId file_info(file_id, my_source, uint_file_size);
    const std::vector<uint8_t> request = dfd::createIndexRequest(file_info);

    if (request.empty()) {
        std::cerr << "Failed to create index request.\n";
        return;
    }

    auto sockOpt = dfd::openSocket(false, 0);
    if (!sockOpt.has_value()) {
        std::cerr << "Failed to open client socket.\n";
        return;
    }

    auto [client_socket_fd, ephemeral_port] = sockOpt.value();
    std::cout << "Client socket_fd = " << client_socket_fd
              << ", ephemeral port = " << ephemeral_port << std::endl;

    SourceInfo server_info;
    server_info.ip_addr = server_ip_;
    server_info.port    = static_cast<uint16_t>(server_port_);

    if (dfd::connect(client_socket_fd, server_info) == -1) {
        std::cerr << "Failed to connect to server ("
                  << server_info.ip_addr << ":" << server_info.port << ").\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }

    std::cout << "Successfully connected to server at "
              << server_info.ip_addr << ":" << server_info.port << "\n";


    if(dfd::sendMessage(client_socket_fd, request) == EXIT_FAILURE) {
        std::cerr << "Failed to send index request.\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }


    std::vector<uint8_t> buffer;
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 750000;
    if(dfd::recvMessage(client_socket_fd, buffer, tv) == -1) {
        std::cerr << "Failed to receive index response.\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }


    dfd::closeSocket(client_socket_fd);


    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        shared_files_[file_id] = file_name;
    }

    std::cout << "File '" << file_name
              << "' is now shared with file_id = " << file_id << ".\n";
}

//------------------------------------------------------------------------------
// Private: handle "download <file>"
//------------------------------------------------------------------------------
void P2PClient::handleDownload(const uint64_t file_uuid)
{
    auto peers = findFilePeers(file_uuid);
    if (peers.empty()) {
        std::cout << "No peers found for file '" << file_uuid << "'.\n";
        return;
    }

    auto [peer_ip, ip_addr, peer_port] = peers[0];
    std::cout << "Attempting to download '" << file_uuid
              << "' from peer " << peer_ip << ":" << peer_port << "...\n";

    auto sockOpt = dfd::openSocket(false, 0);
    if (!sockOpt.has_value()) {
        std::cerr << "[handleDownload] Failed to open client socket.\n";
        return;
    }
    auto [client_socket_fd, ephemeral_port] = sockOpt.value();
    std::cout << "[handleDownload] client_socket_fd=" << client_socket_fd
              << ", ephemeral_port=" << ephemeral_port << "\n";

    SourceInfo peer_info;
    peer_info.ip_addr = peer_ip;
    peer_info.port    = static_cast<uint16_t>(peer_port);

    if (dfd::connect(client_socket_fd, peer_info) == -1) {
        std::cerr << "[handleDownload] Failed to connect to peer ("
                  << peer_ip << ":" << peer_port << ").\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }
    std::cout << "[handleDownload] Successfully connected to peer.\n";

    std::vector<uint8_t> request = createDownloadInit(file_uuid, std::nullopt); // TODO: check if this is correct usage

    if (dfd::sendMessage(client_socket_fd, request) == EXIT_FAILURE) {
        std::cerr << "[handleDownload] Failed to send GET request.\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }

    std::string filename = "downloaded_" + std::to_string(file_uuid);

    std::filesystem::path out_path = std::filesystem::current_path();
    out_path /= "storage";
    out_path /= filename;

    std::ofstream out_file(out_path, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "[handleDownload] Could not open output file at '"
                  << out_path.string() << "'.\n";
        dfd::closeSocket(client_socket_fd);
        return;
    }

    while (true) {
        std::vector<uint8_t> buffer;
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 750000;

        ssize_t bytes_received = recvMessage(client_socket_fd, buffer, tv);
        if (bytes_received <= 0) {
            break; // assuming 0 => peer closed connection; -1 => error/timeout
        }

        auto [chunk_index, data_chunk] = parseDataChunk(buffer);
        if (chunk_index == SIZE_MAX && data_chunk.empty()) {
            std::cerr << "[handleDownload] Received invalid data chunk.\n";
            break;
        }

        out_file.write(
            reinterpret_cast<const char*>(data_chunk.data()),
            static_cast<std::streamsize>(data_chunk.size())
        );

    }

    out_file.close();
    dfd::closeSocket(client_socket_fd);

    std::cout << "[handleDownload] Download complete. File saved as '"
                << filename << "'\n";
}

//------------------------------------------------------------------------------
// Private: handle "remove <file>"
//------------------------------------------------------------------------------
// void P2PClient::handleRemove(const std::string& file_name)
// {
//     // Find which UUID belongs to this filename
//     std::string target_uuid;
//     {
//         std::lock_guard<std::mutex> lock(share_mutex_);
//         for (auto& kv : shared_files_) {
//             if (kv.second == file_name) {
//                 target_uuid = kv.first;
//                 break;
//             }
//         }
//     }

//     if (target_uuid.empty()) {
//         std::cout << "We are not currently sharing file: "
//                   << file_name << "\n";
//         return;
//     }

//     // Remove from the server index
//     if (removeFile(target_uuid)) {
//         // Remove from local map
//         {
//             std::lock_guard<std::mutex> lock(share_mutex_);
//             shared_files_.erase(target_uuid);
//         }
//         std::cout << "Removed file '" << file_name
//                   << "' from server index.\n";
//     } else {
//         std::cerr << "Failed to remove file '" << file_name
//                   << "' from server index.\n";
//     }
// }

//------------------------------------------------------------------------------
// Private: print help info
//------------------------------------------------------------------------------
void P2PClient::printHelp()
{
    std::cout << "Available commands:\n";
    std::cout << "  index <filename>    - Register/share <filename>\n";
    std::cout << "  download <filename> - Download <filename> from a peer\n";
    // std::cout << "  remove <filename>   - Remove <filename> from the server\n";
    std::cout << "  help                - Show this message\n";
    std::cout << "  exit                - Quit the client\n";
}

//------------------------------------------------------------------------------
// Private: find peers for a given file
//------------------------------------------------------------------------------
std::vector<SourceInfo>
P2PClient::findFilePeers(uint64_t file_id)
{
    auto sockOpt = dfd::openSocket(false, 0);
    if (!sockOpt.has_value()) {
        std::cerr << "[findFilePeers] Failed to open client socket.\n";
        return {}; // returns empty vector
    }

    auto [client_socket_fd, ephemeral_port] = sockOpt.value();
    std::cout << "[findFilePeers] client_socket_fd=" << client_socket_fd
              << ", ephemeral_port=" << ephemeral_port << "\n";

    SourceInfo server_info;
    server_info.ip_addr = server_ip_;
    server_info.port    = static_cast<uint16_t>(server_port_);

    if (dfd::connect(client_socket_fd, server_info) == -1) {
        std::cerr << "[findFilePeers] Failed to connect to server at "
                  << server_info.ip_addr << ":" << server_info.port << "\n";
        dfd::closeSocket(client_socket_fd);
        return {};
    }
    std::cout << "[findFilePeers] Connected to "
              << server_info.ip_addr << ":" << server_info.port << "\n";


    std::vector<uint8_t> request = createSourceRequest(file_id);

    if (dfd::sendMessage(client_socket_fd, request) == EXIT_FAILURE) {
        std::cerr << "[findFilePeers] Failed to send SOURCE request.\n";
        dfd::closeSocket(client_socket_fd);
        return {};
    }

    std::vector<uint8_t> peers;

    while (true) {
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 750000;

        ssize_t bytes_received = dfd::recvMessage(client_socket_fd, peers, tv);
        if (bytes_received <= 0) {
            break; // assuming 0 => peer closed connection; -1 => error/timeout
        }
    }

    std::vector<SourceInfo> source_list = parseSourceList(peers);

    dfd::closeSocket(client_socket_fd);
    return source_list;
}

//------------------------------------------------------------------------------
// Private: start listening for peer connections
//------------------------------------------------------------------------------
void P2PClient::startListening()
{
    if (listen_sock_ >= 0) {
        return;
    }

    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ < 0) {
        std::cerr << "[startListening] Could not create socket.\n";
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0; // ephemeral port
    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0) {
        std::cerr << "[startListening] Bind failed.\n";
        closeSocket(listen_sock_);
        listen_sock_ = -1;
        return;
    }

    if (listen(listen_sock_, 5) < 0) {
        std::cerr << "[startListening] Listen failed.\n";
        closeSocket(listen_sock_);
        listen_sock_ = -1;
        return;
    }

    listener_thread_ = std::thread(&P2PClient::listeningLoop, this);
}

//------------------------------------------------------------------------------
// Private: the main loop that accepts connections
//------------------------------------------------------------------------------
void P2PClient::listeningLoop()
{
    std::cout << "[listeningLoop] Listening for incoming peer connections...\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int client_sock = accept(listen_sock_,
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &client_size);
        if (client_sock < 0) {
            if (!is_running_) {
                // If we're shutting down, break.
                break;
            }
            continue;
        }

        std::thread t(&P2PClient::handlePeerRequest, this, client_sock);
        t.detach();
    }

    closeSocket(listen_sock_);
    listen_sock_ = -1;
}

//------------------------------------------------------------------------------
// Private: handle a single peer request
//------------------------------------------------------------------------------
void P2PClient::handlePeerRequest(int client_socket_fd)
{
    std::vector<uint8_t> buffer;
    while (true) {
        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 750000;

        ssize_t bytes_received = dfd::recvMessage(client_socket_fd, buffer, tv);
        if (bytes_received <= 0) {
            // 0 => peer closed connection; -1 => error/timeout
            break;
        }
    }


    auto [uuid, c_size] = parseDownloadInit(buffer);


    std::string file_name;
    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        auto it = shared_files_.find(uuid);
        if (it != shared_files_.end()) {
            file_name = it->second;
        }
    }

    if (file_name.empty()) {

        std::string msg = "ERROR: No such file UUID: " + std::to_string(uuid) + "\n";
        std::vector<uint8_t> fail_vec(msg.begin(), msg.end());
        dfd::sendMessage(client_socket_fd, fail_vec);
        dfd::closeSocket(client_socket_fd);
        return;
    }


    std::filesystem::path file_path = std::filesystem::current_path()
                                      / "storage"
                                      / file_name;


    std::ifstream in_file(file_path, std::ios::binary);
    if (!in_file.is_open()) {

        std::string msg = "ERROR: Cannot open file: " + file_name + "\n";
        std::vector<uint8_t> fail_vec(msg.begin(), msg.end());
        dfd::sendMessage(client_socket_fd, fail_vec);
        dfd::closeSocket(client_socket_fd);
        return;
    }

    if (c_size.has_value()) {
        dfd::setChunkSize(c_size.value());
    }

    std::optional<ssize_t> file_size_opt;
    try {
        file_size_opt = fileSize(file_path);
    } catch(const std::filesystem::filesystem_error& e) {
        std::cerr << "[handlePeerRequest] Failed to get file size for '"
                  << file_path << "': " << e.what() << std::endl;
    }

    if(!file_size_opt.has_value()) {
        std::string msg = "ERROR: Could not determine size of: " + file_name + "\n";
        std::vector<uint8_t> fail_vec = createFailMessage(msg);
        dfd::sendMessage(client_socket_fd, fail_vec);
        dfd::closeSocket(client_socket_fd);
        return;
    }

    std::optional<size_t> num_chunks = fileChunks(file_size_opt.value());
    if (!num_chunks.has_value()) {
        std::string msg = "ERROR: Failed to compute number of chunks for: " + file_name + "\n";
        std::vector<uint8_t> fail_vec = createFailMessage(msg);
        dfd::sendMessage(client_socket_fd, fail_vec);
        dfd::closeSocket(client_socket_fd);
        return;
    }

    for (size_t chunk_idx = 0; chunk_idx < num_chunks.value(); ++chunk_idx)
    {
        std::vector<uint8_t> chunk_buffer;

        std::optional<ssize_t> read_bytes =
            packageFileChunk(file_path, chunk_buffer, chunk_idx);
        if(!read_bytes.has_value() || read_bytes.value() < 0) {
            std::string msg = "ERROR: Failed to read chunk " + std::to_string(chunk_idx) + "\n";
            std::vector<uint8_t> fail_vec = createFailMessage(msg);
            dfd::sendMessage(client_socket_fd, fail_vec);
            break;
        }

        auto data_chunk_message = createDataChunk({chunk_idx, chunk_buffer});
        if (data_chunk_message.empty()) {
            std::string msg = "ERROR: Failed to package chunk " + std::to_string(chunk_idx) + "\n";
            std::vector<uint8_t> fail_vec = createFailMessage(msg);
            dfd::sendMessage(client_socket_fd, fail_vec);
            break;
        }

        if (dfd::sendMessage(client_socket_fd, data_chunk_message) == EXIT_FAILURE) {
            std::cerr << "[handlePeerRequest] Send failure on chunk "
                      << chunk_idx << ".\n";
            break;
        }
    }

    // TODO: finish sending
    // {
    //     std::string done_msg = "FINISH_OK\n";
    //     std::vector<uint8_t> done_vec(done_msg.begin(), done_msg.end());
    //     dfd::sendMessage(client_socket_fd, done_vec);
    // }

    dfd::closeSocket(client_socket_fd);
}

//------------------------------------------------------------------------------
// Private: stop listening and clean up
//------------------------------------------------------------------------------
void P2PClient::stopAllSharing()
{
    is_running_ = false;

    if (listen_sock_ >= 0) {
        shutdown(listen_sock_, SHUT_RDWR);
        closeSocket(listen_sock_);
        listen_sock_ = -1;
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
}

//------------------------------------------------------------------------------
// Private: simplistic approach to get "our" IP
//------------------------------------------------------------------------------
std::string P2PClient::getLocalIPAddress()
{
    // Very simplistic. Usually you'd query the actual network interface.
    return "127.0.0.1";
}

//------------------------------------------------------------------------------
// Private: figure out which port we're listening on
//------------------------------------------------------------------------------
int P2PClient::getListeningPort()
{
    if (listen_sock_ < 0) {
        return 0; // not listening
    }
    sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(listen_sock_, reinterpret_cast<sockaddr*>(&sin), &len) == -1) {
        return 0;
    }
    return ntohs(sin.sin_port);
}

} // namespace dfd
