#pragma once

#include "sourceInfo.hpp"
#include <optional>
#include <vector>

namespace dfd {

//RECV TIMEOUT
#define RECV_TIMEOUT_SEC 1 // seconds
#define RECV_TIMEOUT_USEC 750000 // microseconds

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * connectToSource
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts connecting to a IP/port combo specified inside connect_to. If no
 *    connection can be established within the connection_timeout window,
 *    returns an error.
 *
 * Takes:
 * -> connect_to:
 *    The socket to connect to.
 * -> connection_timeout:
 *    How long to attempt the connection for.
 *
 * Returns:
 * -> On success:
 *    The connected socket fd.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int connectToSource(const  SourceInfo connect_to,
                    struct timeval   connection_timeout);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * sendOkay
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to send a message over the socket provided.
 *
 * Takes:
 * -> sock:
 *    The socket to send over.
 * -> message:
 *    The message buffer to send.
 *
 * Returns:
 * -> On success:
 *    True 
 * -> On failure:
 *    False 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
bool sendOkay(int                         sock,
              const std::vector<uint8_t>& message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * recvOkay
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to receive a message into a provided message buffer. This buffer
 *    is cleared by this function before any attempt to receive. If a message
 *    could be received, it checks the code of the message against the expected
 *    code, and will return an error accordingly. The message contents are
 *    preserved in the case that there's a want to preserve the FAIL message to
 *    print out the message.
 *
 * Takes:
 * -> sock:
 *    The socket to receive the message from.
 * -> buffer:
 *    The destination buffer to store the received contents into.
 * -> expected_code:
 *    The 1-byte code that the message should have. Should be provided pulled
 *    from messageFormatting. 
 * -> timeout:
 *    A timeout for receiving the message back.
 *
 * Returns:
 * -> On success:
 *    True
 * -> On failure:
 *    False
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
bool recvOkay(int                           sock,
              std::vector<uint8_t>&         buffer,
              const uint8_t                 expected_code,
              std::optional<struct timeval> timeout=std::nullopt);


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * sendAndRecv
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Combines the above two functions into one call.
 *
 * Takes:
 * -> sock_fd:
 *    The socket to send/recv through/from.
 * -> out:
 *    The message being sent.
 * -> in:
 *    A buffer to receive the message into.
 * -> expected_code:
 *    The message code to check for.
 * -> timeout:
 *    A timeout for the receive.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int sendAndRecv(int                           sock_fd,
                const  std::vector<uint8_t>&  out,
                       std::vector<uint8_t>&  in,
                const  uint8_t                expected_code,
                std::optional<struct timeval> timeout=std::nullopt);

} //dfd
