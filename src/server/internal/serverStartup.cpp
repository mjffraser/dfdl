#include "server/internal/serverStartup.hpp"
#include "server/internal/syncing.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include <iostream>
#include <ostream>
#include "networking/fileParsing.hpp"
#include "sourceInfo.hpp"
#include "server/internal/db.hpp"
#include "networking/fileParsing.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "networking/internal/fileParsing/fileUtil.hpp"
#include <mutex>
#include "client/internal/internal/internal/clientNetworking.hpp"



namespace dfd {




    
////////////////////////////////////////////////////////////
//RELATED TO STARTUP SYNCRONIZATION
//note: all of this is from old server and likely needs adaption
////////////////////////////////////////////////////////////

int migrationHandshake(const SourceInfo&           server,
                             std::vector<uint8_t>& response_buff,
                             uint64_t&             f_size,
                             struct timeval        timeout) {

    int sock = connectToSource(server, timeout); 
    if (sock < 0) {
        std::cerr << "[ERR] Failed to connect to target server for db migration." << std::endl;
        return EXIT_FAILURE;
    }

    int res = sendAndRecv(sock,
                          {DOWNLOAD_INIT},
                          response_buff,
                          DOWNLOAD_CONFIRM,
                          timeout);
    if (res == EXIT_FAILURE) {
        std::cerr << "[ERR] Failed to send/recv handshake for db migration." << std::endl;
        closeSocket(sock);
        return EXIT_FAILURE;
    }

    auto [size, name] = parseDownloadConfirm(response_buff);
    if (size == 0 || name != "temp.db") {
        std::cerr << "[ERR] Failed to parseDownloadConfirm for db migration." << std::endl;
        closeSocket(sock);
        return EXIT_FAILURE;
    }
    f_size = size;

    return sock;
}

int downloadDBChunk(int                sock,
                    const size_t       chunk_index,
                    const std::string& f_name,
                    struct timeval     response_timeout) {
    //Try to receive chunk
    std::vector<uint8_t> chunk_req = createChunkRequest(chunk_index);
    std::vector<uint8_t> chunk_data;
    if (EXIT_FAILURE == sendAndRecv(sock,
                                    chunk_req,
                                    chunk_data,
                                    DATA_CHUNK,
                                    response_timeout)) {
        std::cerr << "[ERR] Failed to send/recv FileChunk for db migration." << std::endl;
        return EXIT_FAILURE;
    }

    //store received datachunk
    DataChunk dc = parseDataChunk(chunk_data);
    if (dc.first == SIZE_MAX) {
        std::cerr << "[ERR] Failed to parse FileChunk at db migration." << std::endl; 
        return EXIT_FAILURE;
    }

    if (EXIT_FAILURE == unpackFileChunk(f_name,
                                        dc.second,
                                        dc.second.size(),
                                        dc.first)) {
        std::cerr << "[ERR] Failed to unpack FileChunk at db migration." << std::endl;                                    
        return EXIT_FAILURE;
        }

    return EXIT_SUCCESS;
}

//sends database backup to the new server
int databaseSendNS(int socket_fd) {
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 5;

    //temp path 2 store DB backup
    const std::string temp_path = "temp.db";

    auto f_size_op = fileSize(temp_path);
    if (!f_size_op) {
        std::vector<uint8_t> fail_buff = createFailMessage("Database did not backup.");
        tcp::sendMessage(socket_fd, fail_buff);
        return EXIT_FAILURE;
    }
    ssize_t f_size = f_size_op.value();
    std::vector<uint8_t> confirm_buff = createDownloadConfirm(f_size, "temp.db");
    tcp::sendMessage(socket_fd, confirm_buff);

    std::vector<uint8_t> ack_buff;
    while (recvOkay(socket_fd, ack_buff, REQUEST_CHUNK, timeout)) {
        size_t chunk_id = parseChunkRequest(ack_buff); 

        //read chunk
        std::vector<uint8_t> chunk;
        auto res = packageFileChunk(temp_path, chunk, chunk_id);
        if (!res) {
            //could not read file for some reason
            std::vector<uint8_t> fail_msg = createFailMessage("Sorry, file appears to be unavailable.");
            sendOkay(socket_fd, fail_msg);
            return EXIT_FAILURE;
        }

        //send chunk
        chunk.resize(res.value());
        DataChunk dc = {chunk_id, chunk};
        std::vector<uint8_t> chunk_msg = createDataChunk(dc);
        double X=((double)rand()/(double)RAND_MAX);
        if (!sendOkay(socket_fd, chunk_msg)) {
            return EXIT_FAILURE;
        }

    }

    return EXIT_SUCCESS;
}
//called by new server to receive and merge database
int databaseReciveNS(const SourceInfo& server) {
    setDownloadDir(std::filesystem::current_path());
    std::unique_ptr<std::ofstream> file_out = nullptr;

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 5;

    std::vector<uint8_t> response_buff;
    uint64_t f_size;
    std::string f_name = "temp.db";

    // handshake
    int sock = migrationHandshake(server, response_buff, f_size, timeout);
    if (sock == EXIT_FAILURE) {
        std::cerr << "[ERR] Initial db migration handshake failed." << std::endl;
        return EXIT_FAILURE;
    }

    // download & unpack first chunk
    if (EXIT_FAILURE == downloadDBChunk(sock, 0, f_name, timeout)) {
        std::cerr << "[ERR] Failed to download first FileChunk." << std::endl;
        closeSocket(sock);
        return EXIT_FAILURE;
    }

    sendOkay(sock, {FINISH_DOWNLOAD});
    closeSocket(sock);

    file_out = openFile(f_name);
    if (file_out == nullptr){
        std::cerr << "[ERR] Failed to open file at db migration." << std::endl;
        return EXIT_FAILURE;
    }


    auto chunks_in_file_opt = fileChunks(f_size);
    if (!chunks_in_file_opt) {
        std::cerr << "[ERR] Received erroneous file size at db migration." << std::endl;
        return EXIT_FAILURE;
    }
    // download remaining file chunks
    size_t f_chunks = chunks_in_file_opt.value();
    if (f_chunks > 1) {
        std::vector<uint8_t> response_buff1;
        int sock1 = migrationHandshake(server, response_buff1, f_size, timeout);
        if (sock1 == EXIT_FAILURE) {
            std::cerr << "[ERR] download db migration handshake failed." << std::endl;
            return EXIT_FAILURE;
        }
        
        for (size_t i = 1; i < f_chunks; ++i){
            if (EXIT_FAILURE == downloadDBChunk(sock, i, f_name, timeout)) {
                std::cerr << "[ERR] Failed to download remaining FileChunks." << std::endl;
                closeSocket(sock);
                return EXIT_FAILURE;
            }
            assembleChunk(file_out.get(), f_name, i);
        }
        sendOkay(sock1, {FINISH_DOWNLOAD});
        closeSocket(sock1);
    }
    saveFile(std::move(file_out));
    std::cout << "db downloaded." << std::endl;

    return EXIT_SUCCESS;
}

//send a backup of write req
void massWriteSend(SourceInfo& new_server, std::queue<std::vector<uint8_t>> msg_queue) {
    //check empty
    if (msg_queue.empty()) {
        return;
    }

    //loop thru each qued msg and send a forwarded version if its a valid msg to send (big ol switch)
    while (!msg_queue.empty()) {
        auto spoof_sock = openSocket(false, 0);
        if (!spoof_sock) {
            return;
        }

        auto& [spoof_fd, spoof_port] = spoof_sock.value();
        //check and pop start of que
        std::vector<uint8_t> msg = msg_queue.front();
        msg_queue.pop();
        //this is what we send
        std::vector<uint8_t> the_send;
    
        //make sure msg is not empty and get msg type
        if (msg.empty()){
            continue;
        }
        uint8_t msg_type = msg[0];
    
        switch (msg_type) {
            //turn msg to forward if not already
            case DROP_REQUEST:
                if (createForwardDrop(msg) == EXIT_SUCCESS)
                    the_send = msg; // modified in place
                break;
            case REREGISTER_REQUEST:
                if (createForwardRereg(msg) == EXIT_SUCCESS)
                    the_send = msg;
                break;
            case INDEX_REQUEST:
                if (createForwardIndex(msg) == EXIT_SUCCESS)
                    the_send = msg;
                break;
            case DROP_FORWARD:
                the_send = msg;
                break;
            case REREGISTER_FORWARD:
                the_send = msg;
                break;
            case INDEX_FORWARD:
                the_send = msg;
                break;
            default:
                //ignore unknown or irrelevant message types
                continue;
        }
    
        if (!the_send.empty()) {
            if (tcp::sendMessage(spoof_fd, the_send) != EXIT_SUCCESS) {
                std::cerr << "mass send, msg failed to send" << static_cast<int>(msg_type) << "\n";
                //not breaking on fail
            }
        }
    }
}

////////////////////////////////////////////////////////////
//BEGINS SERVER SYNC PROCCESS
//note: essentially the main function that will call sendNS and massWriteSend
////////////////////////////////////////////////////////////


void joinNetwork(const SourceInfo&           known_server,
                    Database*                db,
                    std::vector<SourceInfo>& known_servers,
                    std::mutex&              knowns_mtx,
                    SourceInfo               our_server) {


    // Extract IP and port
    std::string server_ip = known_server.ip_addr;
    uint16_t server_port  = known_server.port;

    //open client TCP socket (unsure if server_port is right or if I should default this to somethin)
    auto socket = openSocket(false, server_port);
    if (!socket) {
        std::cerr << "Failed to open client socket for setup.\n";
        return;
    }

    //our client socket we are using
    int client_sock = socket.value().first;

    //attempt to connect and catch any errors and output error
    if (tcp::connect(client_sock, known_server) == EXIT_FAILURE) {
        std::cerr << "couldent connect to sister server @ IP:" << server_ip << "PORT:" << server_port << "\n";
        closeSocket(client_sock);
        return;
    }
    //connection successful
    std::cout << "connected to sister server @ IP:" << server_ip << "PORT:" << server_port << "\n";

    //send initial_message setup request message
    std::vector<uint8_t> setup_message = createNewServerReg(our_server);
    if (tcp::sendMessage(client_sock, setup_message) == EXIT_FAILURE) {
        std::cerr << "Failed to send setup message.\n";
        closeSocket(client_sock);
        return;
    }

    //buffer for response
    std::vector<uint8_t> buffer;
    timeval timeout = {5, 0};
    ssize_t read_bytes = tcp::recvMessage(client_sock, buffer, timeout);
    //errorcheck
    if (read_bytes <= 0) {
        std::cerr << "no response from known server.\n";
        closeSocket(client_sock);
        return;
    }

    {
        //lock mutex
        std::lock_guard<std::mutex> lock(knowns_mtx);

        //known_servers = all known severs of connected server+the connected server
        known_servers = parseServerRegResponse(buffer);
        known_servers.push_back(known_server);

        std::cout << "Registered with server network." << std::endl;
        std::cout << "[DEBUG] SERVERS:" << std::endl;
        for (auto& s : known_servers) {
            std::cout << s.ip_addr << " " << s.port << std::endl;
        }

        //close socket
        closeSocket(client_sock);
    }

    if (EXIT_SUCCESS == databaseReciveNS(known_server)) {
        //merge file and current db
        if (EXIT_SUCCESS != db->mergeDatabases("temp.db")) {
            std::cerr << "[ERR] db recive merge fail" << std::endl;
        }

        std::string path = "temp.db";
        deleteFile(path);
    }

    // send ack to target server after db merge.
    int sock = connectToSource(known_server, timeout); 
    if (sock < 0) {
        std::cerr << "[ERR] Failed to connect to target server for migration ack." << std::endl;
        return;
    }
    if (!sendOkay(sock, {MIGRATE_OK})) {
        std::cerr << "[ERR] Failed to send db ack." << std::endl;
    }
}


}//namespace