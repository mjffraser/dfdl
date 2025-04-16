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


//sends database backup to the new server
int databaseSendNS(int socket_fd) {
    timeval timeout;
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



    // while (true) {
    //     std::vector<uint8_t> request;
    //     if (tcp::recvMessage(socket_fd, request, timeout) <= 0) {
    //         break;
    //     }

    //     if (*request.begin() == FINISH_DOWNLOAD)
    //         break;

    //     if (*request.begin() != REQUEST_CHUNK)
    //         break;

    //     size_t chunk = parseChunkRequest(request);
    //     std::vector<uint8_t> c_bytes;
    //     auto c_data = packageFileChunk(temp_path, c_bytes, chunk);
    //     if (!c_data)
    //         break;
        
    //     DataChunk send_to_client = {chunk, c_bytes}; 
    //     std::vector<uint8_t> message = createDataChunk(send_to_client);
    //     tcp::sendMessage(socket_fd, message);
    // }

    //deleteFile(temp_path);

    return EXIT_SUCCESS;
}
//called by new server to receive and merge database
int databaseReciveNS(int socket_fd, Database* db) {
    setDownloadDir(std::filesystem::current_path());

    //set the buffer and timeout
    std::vector<uint8_t> buffer;
    timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 5;

    //recive data and EC
    if (tcp::recvMessage(socket_fd, buffer, timeout) <= 0) {
        std::cerr << "Server did not init server download.\n";
        return EXIT_FAILURE;
    }

    if (*buffer.begin() != DOWNLOAD_CONFIRM) {
        std::cerr << "Server sent rouge reply." << std::endl;
        return EXIT_FAILURE;
    }

    std::pair<uint64_t, std::string> confirm_fields = parseDownloadConfirm(buffer);

    //temp to store imported db
    const std::string temp_path = confirm_fields.second;

    auto chunks_op = fileChunks(confirm_fields.first);
    if (!chunks_op) {
        std::cerr << "bad chunks" << std::endl;
        return EXIT_FAILURE;
    }

    std::unique_ptr<std::ofstream> file;
    //write buffer to the tempfile
    for (size_t i = 0; i < chunks_op.value(); ++i) {
        std::cout << "requesting " << i << std::endl;
        std::vector<uint8_t> chunk_buff;
        std::vector<uint8_t> chunk_req = createChunkRequest(i);
        tcp::sendMessage(socket_fd, chunk_req);
        if (tcp::recvMessage(socket_fd, chunk_buff, timeout) <= 0) {
            std::cerr << "bad chunk download" << std::endl;
            return EXIT_FAILURE;
        }

        DataChunk chunk = parseDataChunk(chunk_buff);
        unpackFileChunk(temp_path, chunk.second, chunk.second.size(), i);
        std::cout << chunk.second.size() << std::endl;
        if (i == 0) {
            file = openFile(temp_path);
        } else {
            assembleChunk(file.get(), temp_path, i); 
        }
    }

    tcp::sendMessage(socket_fd, {FINISH_DOWNLOAD});
    saveFile(std::move(file));

    //merge file and current db
    if (EXIT_SUCCESS != db->mergeDatabases(temp_path)) {
        std::cerr << "db recive merge fail\n";
        return EXIT_FAILURE;
    }

    deleteFile(temp_path);

    //exit success
    return EXIT_SUCCESS;
}
//send a backup of write req
void massWriteSend(SourceInfo& new_server, std::queue<std::vector<uint8_t>> msg_queue) {
    //check empty
    if (msg_queue.empty()) {
        return;
    }

    //loop thru each qued msg and send a forwarded version if its a valid msg to send (this block is awful)
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
        if (msg.empty()) continue;
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
                    Database*                open_db,
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
}


}//namespace