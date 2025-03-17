#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace dfd {

struct SourceInfo;

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * getMyPublicIP
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Returns this machines public facing IPv4 address via curl.
 *
 * Returns:
 * -> On success:
 *    A string with the IPv4 address.
 * -> On failure:
 *    An empty string.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::string getMyPublicIP();

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * openSocket
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens a socket. If no port is selected, uses port 0 and OS assigns a port
 *    for an ephemeral port. In this case the port isn't returned with the
 *    socket_fd as its assumed that this ephemeral socket is only used
 *    for a brief connection. 
 *
 *    If udp is set false (DEFAULT), a TCP socket is opened. If set true, a UDP
 *    socket is created instead. 
 *
 *    If a TCP socket, the tcp:: namespace functions should be used with it.
 *    If a UDP socket, the udp:: namespace functions should be used with it.
 * 
 * Takes:
 * -> is_server
 *    A flag to indicate server socket.
 * -> port
 *    The port to open on, set to 0 if not specified.
 *
 * Returns:
 * -> On success:
 *    A pair of the socket fd and the port it was opened on.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::pair<int, uint16_t>> openSocket(bool     is_server,
                                                   uint16_t port=0,
                                                   bool     udp=false);

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

namespace tcp {

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
 * -> Reads data on the socket into a buffer if it's present. 
 * 
 * Takes:
 * -> socket_fd:
 *    The socket to read the data from.
 * -> buffer:
 *    The container to append the read bytes to.
 * -> timeout:
 *    How long this function attempts to read try_to_recv bytes for before
 *    giving up. 
 *
 * Returns:
 * -> On success:
 *    The number of bytes read.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
ssize_t recvMessage(int                   socket_fd, 
                    std::vector<uint8_t>& buffer, 
                    timeval               timeout);

} //tcp

namespace udp {
    
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * sendMessage
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Given a UDP socket and a destination, sends the data in buffer over the
 *    socket. The data buffer MUST NOT exceed 1472 bytes for a single call. Due
 *    to the UDP and IPv4 headers, 1472 is the maximum amount of data that can
 *    be sent without packet fragmentation. There is ABSOLUTELY ZERO guarantee
 *    that a packet sent via this function will be delivered. This should be
 *    used in combination with the below recvMessage function with a timeout to
 *    detect a missing response from the server.
 *
 * Takes:
 * -> socket_fd:
 *    The UDP socket to send the data from.
 * -> receiver_info:
 *    A SourceInfo object containing a valid port and IP address.
 * -> buffer:
 *    The data to send. MAX LENGTH = 1472 bytes.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE (almost certainly due to bad inputs in receiver_info,
 *                  assuming socket is valid)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int sendMessage(int                         socket_fd, 
                SourceInfo&                 receiver_info,
                const std::vector<uint8_t>& buffer);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * recvMessage
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Receives the next UDP datagram in the buffer, if one exists. Puts the
 *    senders info into sender_info. Puts the data into buffer. Any data in the
 *    buffer prior to this call is almost certainly destroyed/overwritten.
 *    Resizes the buffer to size of received data, so .size() is valid.
 *
 * Takes
 * -> socket_fd:
 *    The bound socket to receive a datagram from. If a server socket is created
 *    with openSocket() the socket is already bound.
 * -> sender_info:
 *    A SourceInfo object to store the senders IP and port.
 * -> buffer:
 *    The buffer to store the data in.
 * -> timeout:
 *    An optional timeout. If std::nullopt is provided this function is
 *    blocking.
 * 
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int recvMessage(int                    socket_fd,
                SourceInfo&            sender_info,
                std::vector<uint8_t>&  buffer,
                std::optional<timeval> timeout);

} //udp

} //dfd
