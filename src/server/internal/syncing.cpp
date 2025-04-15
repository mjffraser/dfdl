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



namespace dfd {


////////////////////////////////////////////////////////////
//RELATED TO MESSAGE FORWARDING
////////////////////////////////////////////////////////////


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


////////////////////////////////////////////////////////////
//ADDITIONAL UTILITY USED TO MODIFY OUR VECTOR OF KNOWN SERVERS
////////////////////////////////////////////////////////////

//removes any SourceInfo from known_servers that appears in failed_servers
void removeFailedServers(std::vector<SourceInfo>& known_servers,
                        const std::vector<SourceInfo>& failed_servers,
                        std::mutex&                     known_server_mtx) {
    for (auto& thing : failed_servers) {
        std::cout << thing.ip_addr << " " << thing.port << " " << thing.peer_id << std::endl; 
    }
    //iterate through known servers
    {
        std::lock_guard<std::mutex> lock(known_server_mtx);
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
}


}//namespace
