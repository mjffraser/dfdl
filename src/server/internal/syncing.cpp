//for some reason syncing needs once in both .cpp and .hpp file idfk
#pragma once

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
int databaseSendNS(const SourceInfo& new_server, Database* db) {
    //temp path 2 store DB backup
    const std::string temppath = "temp.db";

    //backup database and EC
    if (EXIT_SUCCESS != db->backupDatabase(temppath)) {
        std::cerr << "db send backup fail\n";
        return EXIT_FAILURE;
    }

    //read data into a memory buffer
    auto memBuff = readChunkData(temppath);
    if (!memBuff) {
        std::cerr << "db send read fail\n";
        return EXIT_FAILURE;
    }

    //get length and info from buffer
    auto pair = *memBuff;
    size_t& readLen = pair.first;
    std::vector<uint8_t>& readbuff = pair.second;

    //open socket and EC
    auto socketA = openSocket(false, 0);
    if (!socketA) {
        std::cerr << "db send socket no open\n";
        return EXIT_FAILURE;
    }

    //get socket info
    std::pair<int, uint16_t> socketpair = *socketA;
    int sock_fd = socketpair.first;
    //we dont use socketpair.second has the port
    
    //connect and EC
    if (tcp::connect(sock_fd, new_server) != EXIT_SUCCESS) {
        std::cerr << "db send connect fail\n";
        closeSocket(sock_fd);
        return EXIT_FAILURE;
    }

    //send the data
    if (tcp::sendMessage(sock_fd, readbuff) != EXIT_SUCCESS) {
        std::cerr << "db send send failed\n";
        closeSocket(sock_fd);
        return EXIT_FAILURE;
    }

    //cleanup
    closeSocket(sock_fd);
    return EXIT_SUCCESS;
}

//called by new server to receive and merge database
int databaseReciveNS(int socket_fd, Database* db) {
    //set the buffer and timeout
    std::vector<uint8_t> buffer;
    timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 5;

    //recive data and EC
    if (tcp::recvMessage(socket_fd, buffer, timeout) <= 0) {
        std::cerr << "db recive recive fail\n";
        return EXIT_FAILURE;
    }

    //temp to store imported db
    const std::string temppath = "temp.db";

    //write buffer to the tempfile
    if (EXIT_SUCCESS != writeToNewFile(temppath, buffer.size(), buffer)) {
        std::cerr << "db recive write to temp fail\n";
        return EXIT_FAILURE;
    }

    //merge file and current db
    if (EXIT_SUCCESS != db->mergeDatabases(temppath)) {
        std::cerr << "db recive merge fail\n";
        return EXIT_FAILURE;
    }
    //exit success
    return EXIT_SUCCESS;
}
//send a backup of write req
void massWriteSend(const SourceInfo& new_server, std::queue<std::vector<uint8_t>> msg_queue) {
    //check empty
    if (msg_queue.empty()) {
        return;
    }

    //tcp socket
    auto socketA = openSocket(false, 0);
    if (!socketA) {
        std::cerr << "mass send socket fail\n";
        return;
    }

    std::pair<int, uint16_t> socket_pair = *socketA;
    //extract file descriptor
    int sock_fd = socket_pair.first;

    //connect to server
    if (tcp::connect(sock_fd, new_server) != EXIT_SUCCESS) {
        std::cerr << "mass send connect fail\n";
        closeSocket(sock_fd);
        return;
    }

    //loop thru each qued msg and send a forwarded version if its a valid msg to send (this block is awful)
    while (!msg_queue.empty()) {
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
            if (tcp::sendMessage(sock_fd, the_send) != EXIT_SUCCESS) {
                std::cerr << "mass send, msg failed to send" << static_cast<int>(msg_type) << "\n";
                //not breaking on fail
            }
        }
    }

    closeSocket(sock_fd);
}

}//namespace
