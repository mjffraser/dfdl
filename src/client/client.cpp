#include "client/client.hpp"
#include "networking/messageFormatting.hpp"
#include "sourceInfo.hpp"
#include "networking/socket.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <random>
#include <cstdlib>
#include <unistd.h>     // close()
#include <arpa/inet.h>  // inet_pton
#include <netinet/in.h> // sockaddr_in, etc.
#include <sys/socket.h> // socket, connect, etc.

namespace
{
    //--------------------------------------------------------------------------
    // Anonymous (internal) namespace for utility functions not exposed in header
    //--------------------------------------------------------------------------

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * generateUUID
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Generates a simplistic "UUID"-like 32-char hex string.
     *    In production, use an actual UUID library.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    std::string generateUUID()
    {
        static const char hex[] = "0123456789ABCDEF";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::string uuid;
        for (int i = 0; i < 32; ++i) {
            uuid.push_back(hex[dis(gen)]);
        }
        return uuid;
    }

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * closeSocket
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Simple wrapper around close() in case any cross-platform adjustments
     *    are needed later.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    void closeSocket(int sockfd)
    {
        close(sockfd);
    }
} // end anonymous namespace

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
            std::string file_name = command.substr(9);
            handleDownload(file_name);
        } else if (command.rfind("remove ", 0) == 0) {
            // e.g. "remove <file uuid>"
            std::string file_name = command.substr(7);
            handleRemove(file_name);
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
    // ?? should this be created by sourceInfo
    uint64_t file_id = 192210290129192019;
    SourceInfo my_source;
    my_source.peer_id = 123456;
    my_source.ip_addr = getLocalIPAddress();
    my_source.port    = static_cast<uint16_t>(getListeningPort());
    uint64_t file_size = 62;
    // ??

    dfd::FileId file_info(file_id, my_source, file_size);

    const std::vector<uint8_t> request = dfd::createIndexRequest(file_info);

    if(request.empty()) {
        std::cerr << "Failed to create index request.\n";
        return;
    }

    // send request to server
    // ?? not sure how to get socket_fd
    if(dfd::sendMessage(2, request) == EXIT_FAILURE){
        std::cerr << "Failed to send index request.\n";
    }

    // receive ack from server
    std::vector<uint8_t> buffer;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 750000;

    if(dfd::recvMessage(2, buffer, tv) == -1){
        std::cerr << "Failed to recieve index response.\n";
    }

    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        shared_files_[file_id] = file_name;
    }

    std::cout << "File '" << file_name
              << "' is now shared with file_id=" << file_id << ".\n";
}

//------------------------------------------------------------------------------
// Private: handle "download <file>"
//------------------------------------------------------------------------------
void P2PClient::handleDownload(const std::string& file_name)
{
    // 1) Ask the index server for peers
    auto peers = findFilePeers(file_name);
    if (peers.empty()) {
        std::cout << "No peers found for file '" << file_name << "'.\n";
        return;
    }

    // 2) Pick the first peer for simplicity
    auto [peer_ip, peer_port] = peers[0];
    std::cout << "Attempting to download '" << file_name
              << "' from peer " << peer_ip << ":" << peer_port << "...\n";

    // 3) Download from that peer
    downloadFromPeer(peer_ip, peer_port, file_name);
}

//------------------------------------------------------------------------------
// Private: handle "remove <file>"
//------------------------------------------------------------------------------
void P2PClient::handleRemove(const std::string& file_name)
{
    // Find which UUID belongs to this filename
    std::string target_uuid;
    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        for (auto& kv : shared_files_) {
            if (kv.second == file_name) {
                target_uuid = kv.first;
                break;
            }
        }
    }

    if (target_uuid.empty()) {
        std::cout << "We are not currently sharing file: "
                  << file_name << "\n";
        return;
    }

    // Remove from the server index
    if (removeFile(target_uuid)) {
        // Remove from local map
        {
            std::lock_guard<std::mutex> lock(share_mutex_);
            shared_files_.erase(target_uuid);
        }
        std::cout << "Removed file '" << file_name
                  << "' from server index.\n";
    } else {
        std::cerr << "Failed to remove file '" << file_name
                  << "' from server index.\n";
    }
}

//------------------------------------------------------------------------------
// Private: print help info
//------------------------------------------------------------------------------
void P2PClient::printHelp()
{
    std::cout << "Available commands:\n";
    std::cout << "  index <filename>    - Register/share <filename>\n";
    std::cout << "  download <filename> - Download <filename> from a peer\n";
    std::cout << "  remove <filename>   - Remove <filename> from the server\n";
    std::cout << "  help                - Show this message\n";
    std::cout << "  exit                - Quit the client\n";
}

//------------------------------------------------------------------------------
// Private: register file with the index server
//------------------------------------------------------------------------------
bool P2PClient::registerFile(const std::string& uuid,
                             const std::string& file_name)
{
    int sock = connectToServer(server_ip_, server_port_);
    if (sock < 0) {
        std::cerr << "[registerFile] Could not connect to server.\n";
        return false;
    }

    std::string our_ip = getLocalIPAddress();
    int our_port = getListeningPort();

    // "REGISTER <UUID> <filename> <our IP> <our port>"
    std::string message = "REGISTER " + uuid + " " + file_name + " " +
                          our_ip + " " + std::to_string(our_port) + "\n";
    if (!sendMessage(sock, message)) {
        std::cerr << "[registerFile] Failed to send REGISTER.\n";
        ::closeSocket(sock);
        return false;
    }

    std::string resp = recvMessage(sock);
    ::closeSocket(sock);

    if (resp.find("OK") != std::string::npos) {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Private: remove file from the server index
//------------------------------------------------------------------------------
bool P2PClient::removeFile(const std::string& uuid)
{
    int sock = connectToServer(server_ip_, server_port_);
    if (sock < 0) {
        return false;
    }

    std::string message = "REMOVE " + uuid + "\n";
    if (!sendMessage(sock, message)) {
        ::closeSocket(sock);
        return false;
    }

    std::string resp = recvMessage(sock);
    ::closeSocket(sock);

    if (resp.find("OK") != std::string::npos) {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// Private: find peers for a given file
//------------------------------------------------------------------------------
std::vector<std::pair<std::string,int>>
P2PClient::findFilePeers(const std::string& file_name)
{
    std::vector<std::pair<std::string,int>> peers;
    int sock = connectToServer(server_ip_, server_port_);
    if (sock < 0) {
        return peers;
    }

    // "SEARCH <filename>"
    std::string message = "SEARCH " + file_name + "\n";
    if (!sendMessage(sock, message)) {
        ::closeSocket(sock);
        return peers;
    }

    while (true) {
        std::string line = recvMessageLine(sock);
        if (line.empty()) {
            // error or no more data
            break;
        }
        if (line == "END") {
            break;
        }
        if (line.rfind("PEER ", 0) == 0) {
            // remove "PEER "
            line.erase(0, 5);
            // split by space
            auto spacePos = line.find(' ');
            if (spacePos != std::string::npos) {
                std::string ip = line.substr(0, spacePos);
                int port = std::stoi(line.substr(spacePos + 1));
                peers.push_back({ip, port});
            }
        }
    }

    ::closeSocket(sock);
    return peers;
}

//------------------------------------------------------------------------------
// Private: start listening for peer connections
//------------------------------------------------------------------------------
void P2PClient::startListening()
{
    if (listen_sock_ >= 0) {
        // Already listening
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
        ::closeSocket(listen_sock_);
        listen_sock_ = -1;
        return;
    }

    if (listen(listen_sock_, 5) < 0) {
        std::cerr << "[startListening] Listen failed.\n";
        ::closeSocket(listen_sock_);
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

    ::closeSocket(listen_sock_);
    listen_sock_ = -1;
}

//------------------------------------------------------------------------------
// Private: handle a single peer request
//------------------------------------------------------------------------------
void P2PClient::handlePeerRequest(int client_sock)
{
    std::string request_line = recvMessageLine(client_sock);
    if (request_line.rfind("GET ", 0) == 0) {
        // e.g. "GET <UUID> <offset> <length>"
        std::string remainder = request_line.substr(4);
        std::istringstream iss(remainder);

        std::string uuid;
        long offset, length;
        iss >> uuid >> offset >> length;

        std::string file_name;
        {
            std::lock_guard<std::mutex> lock(share_mutex_);
            auto it = shared_files_.find(uuid);
            if (it != shared_files_.end()) {
                file_name = it->second;
            }
        }

        if (!file_name.empty()) {
            sendFileChunk(client_sock, file_name, offset, length);
        } else {
            sendMessage(client_sock, "ERROR no_such_file\n");
        }
    }

    ::closeSocket(client_sock);
}

//------------------------------------------------------------------------------
// Private: stop listening and clean up
//------------------------------------------------------------------------------
void P2PClient::stopAllSharing()
{
    is_running_ = false;

    if (listen_sock_ >= 0) {
        shutdown(listen_sock_, SHUT_RDWR);
        ::closeSocket(listen_sock_);
        listen_sock_ = -1;
    }

    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
}

//------------------------------------------------------------------------------
// Private: Download file data from peer
//------------------------------------------------------------------------------
bool P2PClient::downloadFromPeer(const std::string& peer_ip,
                                 int peer_port,
                                 const std::string& file_name)
{
    int sock = connectToServer(peer_ip, peer_port);
    if (sock < 0) {
        std::cerr << "[downloadFromPeer] Could not connect to " << peer_ip
                  << ":" << peer_port << "\n";
        return false;
    }

    // For example, we pretend we know the UUID is "abc123" for the entire file.
    // In a real system, we'd have a correct mapping of <file_name> -> UUID.
    std::string dummy_uuid = "abc123";
    std::string get_msg = "GET " + dummy_uuid + " 0 -1\n";
    if (!sendMessage(sock, get_msg)) {
        std::cerr << "[downloadFromPeer] Could not send GET request.\n";
        ::closeSocket(sock);
        return false;
    }

    std::string out_name = "downloaded_" + file_name;
    std::ofstream out_file(out_name, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "[downloadFromPeer] Could not open file for writing.\n";
        ::closeSocket(sock);
        return false;
    }

    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    int bytes_read = 0;

    while ((bytes_read = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        out_file.write(buffer, bytes_read);
    }

    out_file.close();
    ::closeSocket(sock);
    std::cout << "Download complete. File saved as " << out_name << "\n";
    return true;
}

//------------------------------------------------------------------------------
// Private: send the chunk [offset, offset+length) for file_name
//------------------------------------------------------------------------------
void P2PClient::sendFileChunk(int sock,
                              const std::string& file_name,
                              long offset,
                              long length)
{
    std::ifstream in_file(file_name, std::ios::binary);
    if (!in_file.is_open()) {
        sendMessage(sock, "ERROR cannot_open_file\n");
        return;
    }

    in_file.seekg(0, std::ios::end);
    long file_size = in_file.tellg();
    if (offset > file_size) {
        sendMessage(sock, "ERROR invalid_offset\n");
        return;
    }
    if (length < 0) {
        length = file_size - offset;
    }
    if (offset + length > file_size) {
        length = file_size - offset;
    }

    in_file.seekg(offset, std::ios::beg);

    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    long bytes_remaining = length;

    while (bytes_remaining > 0) {
        long to_read = (bytes_remaining < BUFFER_SIZE)
                       ? bytes_remaining : BUFFER_SIZE;

        in_file.read(buffer, to_read);
        std::streamsize actual_read = in_file.gcount();
        if (actual_read <= 0) {
            break;
        }
        int sent = send(sock, buffer, static_cast<size_t>(actual_read), 0);
        if (sent <= 0) {
            break;
        }
        bytes_remaining -= sent;
    }
}

//------------------------------------------------------------------------------
// Private: connect to server or return -1
//------------------------------------------------------------------------------
int P2PClient::connectToServer(const std::string& ip, int port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
        ::closeSocket(sockfd);
        return -1;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr),
                sizeof(serv_addr)) < 0) {
        ::closeSocket(sockfd);
        return -1;
    }

    return sockfd;
}

//------------------------------------------------------------------------------
// Private: send a string across a socket
//------------------------------------------------------------------------------
bool P2PClient::sendMessage(int sock, const std::string& msg)
{
    size_t total_sent = 0;
    while (total_sent < msg.size()) {
        ssize_t sent = send(sock, msg.data() + total_sent,
                            msg.size() - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

//------------------------------------------------------------------------------
// Private: read until newline
//------------------------------------------------------------------------------
std::string P2PClient::recvMessageLine(int sock)
{
    std::string result;
    char c;
    while (true) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            // error or connection closed
            break;
        }
        if (c == '\n') {
            break;
        }
        result.push_back(c);
    }
    return result;
}

//------------------------------------------------------------------------------
// Private: read entire response (simplified)
//------------------------------------------------------------------------------
std::string P2PClient::recvMessage(int sock)
{
    std::string result;
    char buffer[1024];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        result = buffer;
    }
    return result;
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
