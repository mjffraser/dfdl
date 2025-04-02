
#include "client/client.hpp"
#include "networking/messageFormatting.hpp"
#include "sourceInfo.hpp"
#include "networking/socket.hpp"
#include "networking/fileParsing.hpp"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <optional>

#include <signal.h>
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
#include <random>

namespace dfd
{

static P2PClient* g_client_ptr = nullptr;

static void signalHandler(int signum) {
    if (signum == 2) {
        std::cout << "\nReceived Ctrl+C signal. Cleaning up and exiting...\n";
        if (g_client_ptr) {
            g_client_ptr->handleSignal();
            exit(0);
        }
    }
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
P2PClient::P2PClient(const std::string& server_ip,
                     int                server_port,
                     const std::string& download_dir,
                     const std::string& listen_addr)
  : am_running(true),
    my_listen_sock(-1),
    my_listen_addr(listen_addr),
    my_download_dir(download_dir)
{
    server_info = findHost(server_ip, server_port);

    g_client_ptr = this;
    signal(2, signalHandler); // SIGINT

    my_uuid = initializeUUID();

    if (my_uuid == 0) {
        std::cerr << "Failed to initialize UUID.\n";
        exit(EXIT_FAILURE);
    }

    if (!download_dir.empty())
        setDownloadDir(download_dir);
    else
        my_download_dir = initDownloadDir().string();

    // Start the listening thread at initialization
    startListening();
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
P2PClient::~P2PClient() {
    removeAllFiles();
    stopAllSharing();
    am_running = false;
}

void P2PClient::setRunning(bool running) {
    am_running = running;
}

//------------------------------------------------------------------------------
// Public: main command loop
//------------------------------------------------------------------------------
void P2PClient::run() {
    std::string command;
    std::cout << "Welcome to P2P Client!\n";
    std::cout << "Your current download directory is: " << my_download_dir << "\n";
    std::cout << "Type 'help' for commands.\n";

    while (am_running) {
        std::cout << "> ";
        if (!std::getline(std::cin, command)) {
            break;
        }

        if (command == "exit") {
            std::cout << "Exiting...\n";
            am_running = false;
        } else if (command == "list") {
            handleList();
        } else if (command.rfind("index ", 0) == 0) {
            // e.g. "index myfile.txt"
            std::string file_name = command.substr(6);
            handleIndex(file_name);
        } else if (command.rfind("download ", 0) == 0) {
            // e.g. "download <file uuid>"
            std::string uuid_str = command.substr(9);
            try {
                uint64_t file_uuid = std::stoull(uuid_str);
                handleDownload(file_uuid);
            } catch (const std::exception& e) {
                std::cerr << "Invalid file UUID format. Please provide a valid numeric ID.\n";
            }
        } else if (command.rfind("drop ", 0) == 0) {
            // e.g. "remove <file uuid>"
            std::string file_name = command.substr(5);
            handleDrop(file_name);
        } else if (command == "help") {
            printHelp();
        } else if (command == "crash") {
            exit(-1);
        } else {
            std::cout << "Unknown command. Type 'help' for usage.\n";
        }
    }
}

void P2PClient::handleSignal() {
    stopAllSharing();
    removeAllFiles();
    am_running = false;
}

int connectToServer(const SourceInfo connect_to) {
    auto sockOpt = openSocket(false, 0);
    if (!sockOpt.has_value()) {
        std::cerr << "Failed to open client socket.\n";
        return -1;
    }

    auto [client_socket_fd, ephemeral_port] = sockOpt.value();
    std::cout << "Client socket_fd = " << client_socket_fd
              << ", ephemeral port = " << ephemeral_port << std::endl;

    if (tcp::connect(client_socket_fd, connect_to) == -1) {
        std::cerr << "Failed to connect to server ("
                  << connect_to.ip_addr << ":" << connect_to.port << ").\n";
        closeSocket(client_socket_fd);
        return -1;
    }

    std::cout << "Successfully connected to server at "
              << connect_to.ip_addr << ":" << connect_to.port << "\n";

    return client_socket_fd;
}

//returns bytes read, -1 if err
ssize_t recvWithTimeout(int sock, std::vector<uint8_t>& buffer) {
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 750000;
    ssize_t read;
    read = tcp::recvMessage(sock, buffer, tv);
    if (read < 0) {
        return -1;
    }

    return read;
}

//tries to send message, reports error and closes socket if issue occurs
bool sendOkay(int sock, const std::vector<uint8_t>& message, const std::string& err_msg) {
    if (tcp::sendMessage(sock, message) != EXIT_SUCCESS) {
        if (!err_msg.empty())
            std::cerr << err_msg << std::endl;
        closeSocket(sock);
        return false;
    }

    return true;
}

bool recvOkay(int sock, std::vector<uint8_t>& buffer, const std::string& err_msg) {
    if (recvWithTimeout(sock, buffer) == -1) {
        if (!err_msg.empty())
            std::cerr << err_msg << std::endl;
        closeSocket(sock);
        return false;
    }

    return true;
}

bool checkCode(int sock, const std::vector<uint8_t> buffer, const uint8_t code, bool do_err) {
    if (buffer.size() < 1 || buffer[0] != code) {
        if (buffer[0] == FAIL) {
            if (do_err) std::cerr << "ERROR: " << parseFailMessage(buffer) << std::endl;
        } else {
            if (do_err) std::cerr << "Server supplied rouge data." << std::endl;
        }
        closeSocket(sock);
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Private: handle "list"
//------------------------------------------------------------------------------
void P2PClient::handleList() {
    std::lock_guard<std::mutex> lock(share_mutex_);
    if (shared_files_.empty()) {
        std::cout << "No files indexed.\n";
    } else {
        std::cout << "Currently indexed files:\n";
        for (const auto& kv : shared_files_) {
            std::cout << "  File ID: " << kv.first
            << ", Name: " << std::filesystem::path(kv.second).filename().string()
            << "\n";
        }
    }
}

//------------------------------------------------------------------------------
// Private: handle "index <file>"
//------------------------------------------------------------------------------
void P2PClient::handleIndex(const std::string& file_name) {
    std::filesystem::path f_path = file_name;

    //get hash
    uint64_t file_id = sha256Hash(f_path);
    if (file_id == 0) {
        std::cerr << "Failed to compute file_id for '" << file_name << "'. Is your file empty?\n";
        return;
    }

    //get file size
    std::optional<ssize_t> file_size_pkg = fileSize(f_path);
    if (!file_size_pkg.has_value()) {
        std::cerr << "Failed to retrieve file size for: " << f_path << std::endl;
        return;
    }
    uint64_t f_size = file_size_pkg.value(); //ssize_t will be positive

    //start listening thread, since we're indexing
    startListening();
    if (my_listen_sock < 0) {
        std::cerr << "Failed to open listening socket." << std::endl;
        return;
    }

    //my identity to register with the server
    SourceInfo my_source;
    my_source.peer_id = my_uuid;
    my_source.ip_addr = getLocalIPAddress();
    my_source.port    = static_cast<uint16_t>(getListeningPort());

    //file we're indexing
    FileId file_info(file_id, my_source, f_size);
    const std::vector<uint8_t> request = createIndexRequest(file_info);

    if (request.empty()) {
        std::cerr << "Failed to create index request.\n";
        return;
    }

    int client_socket_fd = connectToServer(server_info);
    if (client_socket_fd < 0) {
        SourceInfo new_host = findHost("",0);
        client_socket_fd = connectToServer(new_host);
        if (client_socket_fd < 0)
            return;
        // return; //err reported already
    }

    if (!sendOkay(client_socket_fd, request, "Failed to send index request."))
        return;

    std::vector<uint8_t> buffer;
    if (!recvOkay(client_socket_fd, buffer, "Failed to receive index response."))
        return;

    if (!checkCode(client_socket_fd, buffer, INDEX_OK, true))
        return;

    closeSocket(client_socket_fd);

    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        shared_files_[file_id] = file_name;
    }

    std::cout << "File '" << file_name << "' is now shared with file_id = " << file_id << ".\n";
}

//------------------------------------------------------------------------------
// Private: handle "download <file>"
//------------------------------------------------------------------------------
void P2PClient::handleDownload(const uint64_t file_uuid) {
    if (fileAlreadyExists(my_download_dir, file_uuid)) {
        std::cout << "Skipping download: File already exists in the download directory." << std::endl;
        return;
    }

    // Reset the state for a new download
    download_complete = false;

    auto peers = findFilePeers(file_uuid);
    if (peers.empty()) {
        std::cout << "No peers found for file '" << file_uuid << "'.\n";
        return;
    }

    std::string f_name;
    size_t chunks;

    bool success = false;
    for (size_t peer_index = 0; peer_index < peers.size(); peer_index++) {
        int client_socket_fd = connectToServer(peers[peer_index]);

        std::vector<uint8_t> request = createDownloadInit(file_uuid, std::nullopt);
        if (!sendOkay(client_socket_fd, request, "Failed to send download request.")) {
            continue;
        }

        std::vector<uint8_t> request_ack;
        if (!recvOkay(client_socket_fd, request_ack, "No response from client.")) {
            sendControlRequest(peers[peer_index], file_uuid, server_info);
            continue;
        }

        if (!checkCode(client_socket_fd, request_ack, DOWNLOAD_CONFIRM, true)) {
            continue;
        }

        auto f_info = parseDownloadConfirm(request_ack);
        uint64_t f_size = f_info.first;
        f_name = f_info.second;
        chunks = fileChunks(f_size).value();

        try {
            std::vector<uint8_t> c_zero_req = createChunkRequest(0);
            if (!sendOkay(client_socket_fd, c_zero_req, "Could not init download procedure with client."))
                throw std::runtime_error("Could not init download procedure with client.");

            std::vector<uint8_t> chunk_response;
            if (!recvOkay(client_socket_fd, chunk_response, "Could not receive chunk for download init.")) {
                sendControlRequest(peers[peer_index], file_uuid, server_info);
                throw std::runtime_error("Could not receive chunk for download init.");
            }

            if (!checkCode(client_socket_fd, chunk_response, DATA_CHUNK, "Client sent something that isn't a chunk!"))
                throw std::runtime_error("Client sent something that isn't a chunk!");

            sendOkay(client_socket_fd, {FINISH_DOWNLOAD}, "Client didn't accept download termination. Non-issue on this end.");

            DataChunk c_zero = parseDataChunk(chunk_response);
            if (EXIT_SUCCESS != unpackFileChunk(f_name, c_zero.second, c_zero.second.size(), c_zero.first))
                throw std::runtime_error("Couldn't unpack file chunk. Are write perms blocked?");

            success = true;
            closeSocket(client_socket_fd);
            break;
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << " Retrying with next peer...\n";
        }
    }

    if (!success) {
        std::cerr << "Failed to download first chunk from any peer. Aborting download.\n";
        return;
    }

    std::unique_ptr<std::ofstream> file = openFile(f_name);
    if (!file) {
        std::cerr << "Couldn't start writing file." << std::endl;
        return;
    }

    if (chunks == 1) {
        // If there is only one chunk, no need for worker threads.
        {
            std::unique_lock<std::mutex> lock(done_chunks_mutex);
            assembleChunk(file.get(), f_name, 0);
        }
        saveFile(std::move(file));
        std::cout << "Downloaded file." << std::endl;
        return;
    }

    for (size_t i = 1; i < chunks; ++i) {
        remaining_chunks.push(i);
    }
    std::cerr << "Number of remaining chunks: " << remaining_chunks.size() << std::endl;


    std::vector<std::thread> workers;

    //we want to select a number of concurrent download threads to use
    //we select the minimum of:
    // -> available peers
    // -> hardware thread limitations
    // -> number of chunks that still need downloading (opening 8 threads for 3 chunks is a waste)
    // -> 5 threads
    size_t num_threads = std::min(peers.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
    num_threads        = std::min(num_threads, remaining_chunks.size());
    num_threads        = std::min(num_threads, static_cast<size_t>(5));

    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(
            &P2PClient::workerThread,
            this,
            file_uuid,
            std::ref(peers),
            i
        );
    }

    while (true) {
        {
            std::unique_lock<std::mutex> lock(done_chunks_mutex);
            chunks_ready.wait(lock, [&] {
                return !done_chunks.empty();
            });
            while (!done_chunks.empty()) {
                size_t c = done_chunks.front(); done_chunks.pop();
                assembleChunk(file.get(), f_name, c);
            }

            {
                std::unique_lock<std::mutex> lock2(remaining_chunks_mutex);
                if (remaining_chunks.empty())
                    break;
            }
        }
    }

    for (auto& worker : workers) {
        worker.join();
    }

    while (!done_chunks.empty()) {
        size_t c = done_chunks.front(); done_chunks.pop();
        assembleChunk(file.get(), f_name, c);
    }

    saveFile(std::move(file));
    std::cout << "Downloaded file." << std::endl;
}

void P2PClient::workerThread(const uint64_t file_uuid, const std::vector<dfd::SourceInfo>& peers, size_t thread_ind) {
    while (true) {
        //next chunk that needs downloading
        size_t chunk_index;
        {
            std::unique_lock<std::mutex> lock(remaining_chunks_mutex);
            if (remaining_chunks.empty())
                return;
            chunk_index = remaining_chunks.front();
            remaining_chunks.pop();
        }

        bool success = false;
        size_t attempts = 0;
        // Cycle through peers until we've tried each once.
        size_t peer_count = peers.size();
        size_t peer_index = thread_ind % peer_count;  // start at a different offset per thread
        while (attempts < peer_count) {
            int download_from = connectToServer(peers[peer_index]);
            std::vector<uint8_t> request = createDownloadInit(file_uuid, std::nullopt);
            if (!sendOkay(download_from, request, "IN THREAD -- Failed to send download request.")) {
                peer_index = (peer_index + 1) % peer_count;
                attempts++;
                continue;
            }

            std::vector<uint8_t> request_ack;
            if (!recvOkay(download_from, request_ack, "IN THREAD -- No response from client with port ." + std::to_string(peers[peer_index].port))) {
                sendControlRequest(peers[peer_index], file_uuid, server_info);
                peer_index = (peer_index + 1) % peer_count;
                attempts++;
                continue;
            }

            if (!checkCode(download_from, request_ack, DOWNLOAD_CONFIRM, true)) {
                peer_index = (peer_index + 1) % peer_count;
                attempts++;
                continue;
            }

            auto f_info = parseDownloadConfirm(request_ack);
            uint64_t f_size = f_info.first;
            std::string f_name = f_info.second;

            if (downloadChunk(download_from, f_name, f_size, chunk_index)) {
                success = true;
                sendOkay(download_from, {FINISH_DOWNLOAD}, "");
                closeSocket(download_from);
                break;
            } else {
                sendControlRequest(peers[peer_index], file_uuid, server_info);
            }

            closeSocket(download_from);
            peer_index = (peer_index + 1) % peer_count;
            attempts++;
        }

        if (!success) {
            std::unique_lock<std::mutex> lock(remaining_chunks_mutex);
            remaining_chunks.push(chunk_index);
        }
    }
}

bool P2PClient::downloadChunk(int client_socket_fd, const std::string& f_name, uint64_t f_size, size_t chunk_index) {
    std::vector<uint8_t> chunk_req = createChunkRequest(chunk_index);
    if (!sendOkay(client_socket_fd, chunk_req, "Could not send chunk request."))
        return false;

    std::vector<uint8_t> chunk_data;
    if (!recvOkay(client_socket_fd, chunk_data, "Could not receive chunk from client."))
        return false;

    if (!checkCode(client_socket_fd, chunk_data, DATA_CHUNK, true))
        return false;

    DataChunk dc = parseDataChunk(chunk_data);
    unpackFileChunk(f_name, dc.second, dc.second.size(), chunk_index);

    std::cout << "Downloaded chunk " << chunk_index << std::endl;

    {
        std::unique_lock<std::mutex> lock(done_chunks_mutex);
        done_chunks.push(chunk_index);
    }
    chunks_ready.notify_one();
    return true;
}

//------------------------------------------------------------------------------
// Private: handle "remove <file>"
//------------------------------------------------------------------------------
void P2PClient::handleDrop(const std::string& file_name) {
    // Find which UUID belongs to this filename
    uint64_t f_uuid = 0;
    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        for (auto& kv : shared_files_) {
            if (kv.second == file_name) {
                f_uuid = kv.first;
                break;
            }
        }
    }

    if (f_uuid == 0) {
        std::cout << "We are not currently sharing file: "
                  << file_name << "\n";
        return;
    }

    IndexUuidPair id_pair(f_uuid, my_uuid);
    std::vector<uint8_t> drop_buff = createDropRequest(id_pair);

    int client_socket_fd = connectToServer(server_info);
    if (client_socket_fd < 0) {
        SourceInfo new_host = findHost("",0);
        client_socket_fd = connectToServer(new_host);
        if (client_socket_fd < 0)
            return;
        // return; //err reported already
    }

    if (!sendOkay(client_socket_fd, drop_buff, "Failed to send drop request."))
        return;

    std::vector<uint8_t> response_buff;
    if (!recvOkay(client_socket_fd, response_buff, "Failed to recieve drop ack."))
        return;

    if (!checkCode(client_socket_fd, response_buff, DROP_OK, true))
        return;

    {
        std::lock_guard<std::mutex> lock(share_mutex_);
            shared_files_.erase(f_uuid);
    }

    closeSocket(client_socket_fd);
    std::cout << "Dropped " << file_name << std::endl;
}

//------------------------------------------------------------------------------
// Private: print help info
//------------------------------------------------------------------------------
void P2PClient::printHelp() {
    std::cout << "Available commands:\n";
    std::cout << "  list                - List all currently indexed files\n";
    std::cout << "  index <filename>    - Register/share <filename>\n";
    std::cout << "  download <filename> - Download <filename> from a peer\n";
    std::cout << "  drop <filename>     - Remove <filename> from the server\n";
    std::cout << "  help                - Show this message\n";
    std::cout << "  exit                - Quit the client\n";
}

//------------------------------------------------------------------------------
// Private: find peers for a given file
//------------------------------------------------------------------------------
std::vector<SourceInfo> P2PClient::findFilePeers(uint64_t file_id) {
    int client_socket_fd = connectToServer(server_info);
    if (client_socket_fd < 0) {
        return {}; //err already reported
    }

    //ask for sources
    std::vector<uint8_t> request = createSourceRequest(file_id);
    if (!sendOkay(client_socket_fd, request, "Failed to send SOURCE request."))
        return {};

    //recieve the list, possibly empty.
    std::vector<uint8_t> peers;
    if (!recvOkay(client_socket_fd, peers, "Failed when recieving peer list."))
        return {};

    //check we got the correct response
    if (!checkCode(client_socket_fd, peers, SOURCE_LIST, true))
        return {};

    //parse and return, again, possibly empty
    std::vector<SourceInfo> source_list = parseSourceList(peers);
    closeSocket(client_socket_fd);
    return source_list;
}

//------------------------------------------------------------------------------
// Private: start listening for peer connections
//------------------------------------------------------------------------------
void P2PClient::startListening() {
    if (my_listen_sock >= 0) {
        return;
    }

    //open a listener
    auto sock_port = openSocket(true, 0);
    if (sock_port) {
        my_listen_sock = sock_port.value().first;
        my_listen_port = sock_port.value().second;
    } else {
        std::cerr << "[startListening] Could not create and bind socket to index to peers.\n";
        return;
    }

    //start listening for incoming clients
    if (listen(my_listen_sock, 5)) {
        std::cerr << "[startListening] Could not start listening.\n";
        closeSocket(my_listen_sock);
        my_listen_sock = -1;
        return;
    }

    my_listen_thread = std::thread(&P2PClient::listeningLoop, this);
}

//------------------------------------------------------------------------------
// Private: the main loop that accepts connections
//------------------------------------------------------------------------------
void P2PClient::listeningLoop() {
    std::cout << "[listeningLoop] Listening for incoming peer connections...\n";
    std::cout << "PORT: " << my_listen_port << std::endl;

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int client_sock = accept(my_listen_sock,
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &client_size);
        if (client_sock < 0) {
            if (!am_running) {
                // If we're shutting down, break.
                break;
            }
            continue;
        }

        std::thread t(&P2PClient::handlePeerRequest, this, client_sock);
        t.detach();
    }

    closeSocket(my_listen_sock);
    my_listen_sock = -1;
}

//------------------------------------------------------------------------------
// Private: handle a single peer request
//------------------------------------------------------------------------------
void P2PClient::handlePeerRequest(int client_socket_fd) {
    //this is a passive listening service for an indexer
    //it should give feedback to client on error

    //recieve client file request
    std::vector<uint8_t> buffer;
    if (!recvOkay(client_socket_fd, buffer, ""))
        return;

    if (!checkCode(client_socket_fd, buffer, DOWNLOAD_INIT, false))
        return; //we dont want background errors randomly popping into indexing clients
                //ui. just suppress with the print flag

    auto [uuid, c_size] = parseDownloadInit(buffer);
    std::string file_path;
    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        auto it = shared_files_.find(uuid);
        if (it != shared_files_.end()) {
            file_path = it->second;
        }
    }

    //next we test for file existing
    if (file_path.empty()) {
        std::vector<uint8_t> fail_buff = createFailMessage("ERROR: No such file UUID: " + std::to_string(uuid));
        tcp::sendMessage(client_socket_fd, fail_buff);
        closeSocket(client_socket_fd);
        return;
    }

    std::string file_name = std::filesystem::path(file_path).filename();

    if (c_size.has_value()) {
        setChunkSize(c_size.value());
    }

    std::optional<ssize_t> file_size_opt = fileSize(file_path);
    if(!file_size_opt.has_value()) {
        std::vector<uint8_t> fail_vec = createFailMessage("ERROR: Could not determine size of: " + file_name);
        tcp::sendMessage(client_socket_fd, fail_vec);
        closeSocket(client_socket_fd);
        return;
    }

    std::optional<size_t> num_chunks = fileChunks(file_size_opt.value());
    if (!num_chunks.has_value()) {
        std::vector<uint8_t> fail_buff = createFailMessage("ERROR: Failed to compute number of chunks for: " + file_name);
        tcp::sendMessage(client_socket_fd, fail_buff);
        closeSocket(client_socket_fd);
        return;
    }

    //send client confirm message with file size that they're downloading
    std::vector<uint8_t> confirm_msg = createDownloadConfirm(file_size_opt.value(), file_name);
    if (!sendOkay(client_socket_fd, confirm_msg, "Could not send client a download confirmation."))
        return;

    while (true) {
        std::vector<uint8_t> client_req;
        if (!recvOkay(client_socket_fd, client_req, ""))
            return;

        //client is done
        if (client_req[0] == FINISH_DOWNLOAD) {
            tcp::sendMessage(client_socket_fd, {FINISH_OK});
            closeSocket(client_socket_fd);
            return;
        }

        //check we actually got a chunk request
        if (!checkCode(client_socket_fd, client_req, REQUEST_CHUNK, false))
            return;

        //read in requested chunk
        size_t chunk_id = parseChunkRequest(client_req);
        std::vector<uint8_t> chunk_buff;
        auto read = packageFileChunk(file_path, chunk_buff, chunk_id);
        if (!read) {
            auto fail_buff = createFailMessage("Sorry, file appears to be unavailable.");
            tcp::sendMessage(client_socket_fd, fail_buff);
            closeSocket(client_socket_fd);
            return;
        }

        //send data
        chunk_buff.resize(read.value());
        DataChunk dc = {chunk_id, chunk_buff};
        std::vector<uint8_t> send_chunk_buff = createDataChunk(dc);
        if (!sendOkay(client_socket_fd, send_chunk_buff, ""))
            return;
    }

    closeSocket(client_socket_fd);
}

//------------------------------------------------------------------------------
// Private: remove all files from the server
//------------------------------------------------------------------------------
void P2PClient::removeAllFiles() {
    std::vector<uint64_t> file_ids;

    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        for (const auto& kv : shared_files_) {
            file_ids.push_back(kv.first);
        }
    }

    if (file_ids.empty()) {
        return;
    }

    std::cout << "Removing all shared files from server...\n";

    for (const auto& file_id : file_ids) {
        IndexUuidPair id_pair(file_id, my_uuid);
        std::vector<uint8_t> drop_buff = createDropRequest(id_pair);

        int client_socket_fd = connectToServer(server_info);
        if (client_socket_fd < 0) {
            continue;
        }

        if (!sendOkay(client_socket_fd, drop_buff, "")) {
            continue;
        }

        std::vector<uint8_t> response_buff;
        if (!recvOkay(client_socket_fd, response_buff, "")) {
            continue;
        }

        closeSocket(client_socket_fd);
    }

    {
        std::lock_guard<std::mutex> lock(share_mutex_);
        shared_files_.clear();
    }
}

//------------------------------------------------------------------------------
// Private: stop listening and clean up
//------------------------------------------------------------------------------
void P2PClient::stopAllSharing() {
    am_running = false;

    if (my_listen_sock >= 0) {
        shutdown(my_listen_sock, SHUT_RDWR);
        closeSocket(my_listen_sock);
        my_listen_sock = -1;
    }

    if (my_listen_thread.joinable()) {
        my_listen_thread.join();
    }
}

//------------------------------------------------------------------------------
// Private: simplistic approach to get "our" IP
//------------------------------------------------------------------------------
std::string P2PClient::getLocalIPAddress() {
    // Very simplistic. Usually you'd query the actual network interface.
    return my_listen_addr;
}

//------------------------------------------------------------------------------
// Private: figure out which port we're listening on
//------------------------------------------------------------------------------
int P2PClient::getListeningPort() {
    if (my_listen_sock < 0) {
        return 0; // not listening
    }
    sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(my_listen_sock, reinterpret_cast<sockaddr*>(&sin), &len) == -1) {
        return 0;
    }
    return ntohs(sin.sin_port);
}

//------------------------------------------------------------------------------
// Private: initialize client's UUID
//------------------------------------------------------------------------------
uint64_t P2PClient::initializeUUID() {
    std::filesystem::path config_dir = "config";
    std::filesystem::create_directory(config_dir);

    std::filesystem::path uuid_path = config_dir / "uuid";
    uint64_t uuid = 0;

    // Try reading UUID from file
    if (std::ifstream uuid_file(uuid_path, std::ios::binary); uuid_file) {
        uuid_file.read(reinterpret_cast<char*>(&uuid), sizeof(uuid));
        if (uuid_file.gcount() == sizeof(uuid)) {
            std::cout << "Loaded UUID from config: " << uuid << std::endl;
            return uuid;
        }
        std::cout << "Error: UUID file exists but does not contain valid data.\n";
    }

    // Generate new UUID
    if (std::ifstream urandom("/dev/urandom", std::ios::binary); urandom) {
        // TODO: We've manually set byte size to 4, but should be sizeof(uuid) for a full UUID.
        urandom.read(reinterpret_cast<char*>(&uuid), 4);
    }
    if (uuid == 0) { // Fallback if /dev/urandom fails
        std::cout << "Warning: Using random_device as fallback to generate UUID.\n";
        std::random_device rd;
        uuid = (static_cast<uint64_t>(rd()) << 32) | rd();
    }

    // Write new UUID to file
    if (std::ofstream uuid_file(uuid_path, std::ios::binary | std::ios::trunc); uuid_file) {
        uuid_file.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));
        std::cout << "Generated new UUID and saved to config: " << uuid << std::endl;
    } else {
        std::cout << "Error: Could not write new UUID to config file.\n";
    }

    return uuid;
}

//------------------------------------------------------------------------------
// Private: find host from config
//------------------------------------------------------------------------------

SourceInfo P2PClient::findHost(const std::string &ip, int port) {
    std::filesystem::path config_dir = "config";
    std::filesystem::create_directory(config_dir);

    std::filesystem::path host_path = config_dir / "hosts";

    if(ip.length() != 0 && port != 0){
        SourceInfo server_info;
        server_info.ip_addr   = ip;
        server_info.port = port;

        int server_fd = connectToServer(server_info);

        sendOkay(server_fd, {CLIENT_REG}, "Failed to send client registration request.");

        std::vector<uint8_t> response_buff;
        recvOkay(server_fd, response_buff, "Failed to receive response");

        std::vector<SourceInfo> available_servers = parseServerRegResponse(response_buff);

        if (!available_servers.empty()) {

            std::cout << "SERVER RESPONDED: " << '\n';

            std::filesystem::path config_dir = "config";
            std::filesystem::path host_path  = config_dir / "hosts";

            std::ofstream out(host_path, std::ios::trunc);
            if (!out) {
                std::cerr << "Failed to open '" << host_path.string()
                          << "' for writing.\n";
                return server_info;
            }

            for (const auto &s : available_servers) {
                std::cout << "HOST: " << s.ip_addr << " " << s.port << "\n";
                out << s.ip_addr << " " << s.port << "\n";
            }

            return server_info;
        }

        return server_info;
    }

    if (!std::filesystem::exists(host_path)) {
        std::cerr << "[findHost] No server ip specified and no 'hosts' file found in "
                    << config_dir.string() << "\n";
        exit(EXIT_FAILURE);
    }

    std::ifstream file(host_path);
    if (!file.is_open()) {
        std::cerr << "[findHost] Failed to open 'hosts' file at "
                    << host_path.string() << "\n";
        exit(EXIT_FAILURE);
    }

    std::vector<SourceInfo> available_servers;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream iss(line);
        std::string candidate_ip;
        int candidate_port = 0;

        if (!(iss >> candidate_ip >> candidate_port)) {
            // If parsing fails, skip this line
            continue;
        }

        SourceInfo server_info;
        server_info.ip_addr   = candidate_ip;
        server_info.port = candidate_port;

        int server_fd = connectToServer(server_info);

        if (!sendOkay(server_fd, {CLIENT_REG}, "Failed to send client registration request.")) {
            continue;
        }

        std::vector<uint8_t> response_buff;
        if (!recvOkay(server_fd, response_buff, "Failed to receive response")) {
            continue;
        }

        available_servers = parseServerRegResponse(response_buff);

        closeSocket(server_fd);

        if (!available_servers.empty()) {
            std::filesystem::path config_dir = "config";
            std::filesystem::path host_path  = config_dir / "hosts";

            std::ofstream out(host_path, std::ios::trunc);
            if (!out) {
                std::cerr << "Failed to open '" << host_path.string()
                            << "' for writing.\n";
                return server_info;
            }

            for (const auto &s : available_servers) {
                out << s.ip_addr << " " << s.port << "\n";
            }

            return server_info;
        }

        return server_info;
    }

    std::cerr << "Could not make a connection with any server.\n";
    exit(EXIT_FAILURE);
}

//------------------------------------------------------------------------------
// Private: send a control request to the server when a potentially dead peer
//          is detected
//------------------------------------------------------------------------------
void P2PClient::sendControlRequest(SourceInfo peer, uint64_t file_uuid, SourceInfo server_info) {
    std::vector<uint8_t> control_request = createControlRequest(peer, file_uuid);
    int server_fd = connectToServer(server_info);
    if (!sendOkay(server_fd, control_request, "Failed to send control request."))
    {
    }
    std::vector<uint8_t> control_response;
    if (!recvOkay(server_fd, control_response, "Could not receive control response from server."))
    {
    }
    if (!checkCode(server_fd, control_response, CONTROL_OK, "Control response not OK."))
    {
    }
    std::cout << "Control response OK." << std::endl;
}

//------------------------------------------------------------------------------
// Private: checks if a file already exists in the download directory
//------------------------------------------------------------------------------
bool P2PClient::fileAlreadyExists(const std::string& download_dir, const uint64_t file_uuid) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(download_dir)) {
            // Ignore directories, symlinks, etc.
            if (!entry.is_regular_file())
                continue;

            uint64_t existing_file_hash = sha256Hash(entry.path());
            if (existing_file_hash == file_uuid) {
                std::cout << "File already exists: " << entry.path().filename() << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error checking existing files: " << e.what() << std::endl;
    }

    return false;
}

} // namespace dfd
