#include "server/internal/syncing.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include <iostream>
#include <ostream>
#include "networking/fileParsing.hpp"
#include "sourceInfo.hpp"
#include "server/internal/database/db.hpp"
#include "networking/fileParsing.hpp"
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include "networking/internal/fileParsing/fileUtil.hpp"
#include <mutex>



namespace dfd {

ssize_t forwardRegistration(std::vector<uint8_t>& reg_message,
                            const std::vector<SourceInfo>& servers) {
    if (*reg_message.begin() != SERVER_REG)
        return -1;

    if (EXIT_SUCCESS != createForwardServerReg(reg_message))
        return -1;

    ssize_t registered_with = 0;

    for (auto& server : servers) {
        //open socket to talk to server
        auto sock = openSocket(false, 0);
        if (!sock)
            return registered_with;

        auto [server_sock, port] = sock.value();
        if (tcp::connect(server_sock, server) == -1) {
            closeSocket(server_sock);
            continue;
        }

        //we've connected, now send the forwarded server reg
        if (EXIT_SUCCESS != tcp::sendMessage(server_sock, reg_message)) {
            closeSocket(server_sock);
            continue;
        }

        //get response
        std::vector<uint8_t> server_response;
        timeval timeout;
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;
        if (tcp::recvMessage(server_sock, server_response, timeout) < 0) {
            closeSocket(server_sock);
            continue;
        }

        if (*server_response.begin() == FORWARD_SERVER_OK)
            registered_with++;
        closeSocket(server_sock);
    }

    return registered_with;
}

//forwards standard write requests, takes in: message, servers, expected code of the msg, and forward msg creation
//forward request is called by below functions, and should never be called directly
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * forwardRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> sends a write request (INDEX, DROP, REREGISTER) to all known servers as
 *    a forwarded request (INDEX_FORWARD, etc.).
 *
 *    used by other funcctions not directly called
 *
 * Takes:
 * -> initial_msg:
 *    the original request message (must match expected_code)
 * -> servers:
 *    the list of servers to forward to
 * -> expected_code:
 *    expected code
 *
 * Returns:
 * -> a list of servers that failed to acknowledge the forwarded request
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<SourceInfo> forwardRequest(
                        std::vector<uint8_t>& initial_msg,
                        const std::vector<SourceInfo>& servers,
                        uint8_t expected_code) {
    //make sure first byte is expected code
    if (*initial_msg.begin() != expected_code)
        return servers;

    //check if creating forward worked
    if (expected_code == INDEX_REQUEST){
        if (EXIT_SUCCESS != createForwardIndex(initial_msg))
            return servers;
    }else if (expected_code == DROP_REQUEST)
    {
        if (EXIT_SUCCESS != createForwardDrop(initial_msg))
            return servers;
    }else if (expected_code == REREGISTER_REQUEST)
    {
        if (EXIT_SUCCESS != createForwardRereg(initial_msg))
            return servers;
    }

    //stores failed serveres
    std::vector<SourceInfo> failed_servers;

    //loop thru all servers
    for (const auto& server : servers) {
        //bool to check if success
        bool success = false;

        //loop X retrys (currently 2 can be changed)
        for (int a = 0; a < 2 && !success; ++a) {
            //attempt to open a client socket
            auto sock = openSocket(false, 0);
            if (!sock)
                //sock cant open skip retrys
                break;

            auto& [server_sock, port] = sock.value();

            //try connecting to the server
            if (tcp::connect(server_sock, server) == -1) {
                closeSocket(server_sock);
                //skip
                continue;
            }

            //ry sending the message
            if (EXIT_SUCCESS != tcp::sendMessage(server_sock, initial_msg)) {
                closeSocket(server_sock);
                //skip
                continue;
            }

            //prepare to receive the server's response
            std::vector<uint8_t> server_response;
            //set time out currently 2 secounds could be different
            timeval timeout;
            timeout.tv_sec  = 2;
            timeout.tv_usec = 0;

            //receive the response with timeout
            if (tcp::recvMessage(server_sock, server_response, timeout) >= 0) {
                //check response
                if (!server_response.empty() && *server_response.begin() == FORWARD_OK) {
                    success = true;
                }
            }

            //close socket
            closeSocket(server_sock);
        }

        //if server never worked add to failed servers
        if (!success)
            failed_servers.push_back(server);
    }

    //return list of failed servers
    return failed_servers;
}

//calls forwarding index version
std::vector<SourceInfo> forwardIndexRequest(
                        std::vector<uint8_t>& initial_msg,
                        const std::vector<SourceInfo>& servers) {
    if (!servers.empty())
        return forwardRequest(initial_msg, servers, INDEX_REQUEST);
    return {};
}

//calls forwarding idrop version
std::vector<SourceInfo> forwardDropRequest(
                        std::vector<uint8_t>& initial_msg,
                        const std::vector<SourceInfo>& servers) {
    if (!servers.empty())
        return forwardRequest(initial_msg, servers, DROP_REQUEST);
    return {};
}

//calls forwarding rereg version
std::vector<SourceInfo> forwardReregRequest(
                        std::vector<uint8_t>& initial_msg,
                        const std::vector<SourceInfo>& servers) {
    if (!servers.empty())
        return forwardRequest(initial_msg, servers, REREGISTER_REQUEST);
    return {};
}

//removes any SourceInfo from known_servers that appears in failed_servers
void removeFailedServers(std::vector<SourceInfo>& known_servers,
                        const std::vector<SourceInfo>& failed_servers) {
    for (auto& thing : failed_servers) {
        std::cout << thing.ip_addr << " " << thing.port << " " << thing.peer_id << std::endl; 
    }
    //iterate through known servers
    for (size_t i = 0; i < known_servers.size(); ) {
        bool match = false;

        //check if current known server is in the list of failed servers
        for (size_t j = 0; j < failed_servers.size(); ++j) {
            if (known_servers[i].ip_addr == failed_servers[j].ip_addr &&
                known_servers[i].port    == failed_servers[j].port) {
                    //set match true and break
                match = true;
                break;
            }
        }
        //if match
        if (match) {
            //remove and stay at i
            known_servers.erase(known_servers.begin() + i);
        } else {
            i++;
        }
    }
}

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

    while (true) {
        std::vector<uint8_t> request;
        if (tcp::recvMessage(socket_fd, request, timeout) <= 0) {
            break;
        }

        if (*request.begin() == FINISH_DOWNLOAD)
            break;

        if (*request.begin() != REQUEST_CHUNK)
            break;

        size_t chunk = parseChunkRequest(request);
        std::vector<uint8_t> c_bytes;
        auto c_data = packageFileChunk(temp_path, c_bytes, chunk);
        if (!c_data)
            break;
        
        DataChunk send_to_client = {chunk, c_bytes}; 
        std::vector<uint8_t> message = createDataChunk(send_to_client);
        tcp::sendMessage(socket_fd, message);
    }

    deleteFile(temp_path);

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

}//namespace
