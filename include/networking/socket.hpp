#pragma once

#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <optional>
#include <sys/socket.h>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace dfd {

struct SourceInfo;

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * openSocket
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens a blocking TCP socket. Uses OS's port 0 to select a free port. 
 *
 * Returns:
 * -> On success:
 *    A pair of the socket fd and the port it was opened on.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::pair<int, uint16_t>> openSocket();

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * closeSocket
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Closes a socket.
 *
 * Takes:
 * -> socket_fd:
 *    The fd of the socket to close.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void closeSocket(int socket_fd);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * connect
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Connect to a socket. Utilized by clients to either connect to a server, or
 *    conenct to a fellow client to download.
 *
 * Takes:
 * -> socket_fd:
 *    The socket to connect with.
 * -> connect_to:
 *    A struct with info on where to connect.
 * 
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS 
 * -> On failure:
 *    -1 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int connect(int socket_fd, const SourceInfo& connect_to);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * listen
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Start listen for incoming connections. 
 *
 * Takes:
 * -> server_fd:
 *    The server socket to start listening on.
 * -> max_pending:
 *    The max clients that can be waiting to be accepted by server. Shouldn't 
 *    need to be very high as the server socket should immediately be handing
 *    off the connection to a thread to take care of.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int listen(int server_fd, int max_pending);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * accept
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Accept an incoming client connection.
 *
 * Takes:
 * -> server_fd:
 *    Server socket that's listening for an incoming connection.
 * -> client_addr:
 *    A sockaddr struct for the new sockets info.
 *
 * Returns:
 * -> On success:
 *    The socket file descriptor of the socket that was opened.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int accept(int server_fd, SourceInfo& client_info);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * sendMessage
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Sends data through a socket. 
 *
 * Takes:
 * -> socket_fd:
 *    The socket to send the data through.
 * -> data:
 *    The data to send.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int sendMessage(int socket_fd, const std::vector<uint8_t>& data);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * recvData
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads data on the socket into a buffer if it's present. Appends to the
 *    buffer rather than overwriting since it very well could take many calls of
 *    this function to recieve the entire message.
 *
 * Takes:
 * -> socket_fd:
 *    The socket to read the data from.
 * -> buffer:
 *    The container to append the read bytes to.
 * -> try_to_recv:
 *    How many bytes to attempt to read.
 * -> timeout:
 *    How long this function attempts to read try_to_recv bytes for before
 *    giving up. 
 *
 * Returns:
 * -> On success:
 *    The number of bytes read. May or may not be equal to try_to_recv.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
ssize_t recvData(int                   socket_fd, 
                 std::vector<uint8_t>& buffer, 
                 size_t                try_to_recv, 
                 timeval               timeout
                );


}
