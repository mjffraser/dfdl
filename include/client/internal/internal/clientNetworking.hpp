#pragma once

#include "sourceInfo.hpp"
#include <vector>

namespace dfd {

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
int connectToSource(const SourceInfo connect_to,
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
 *
 * Returns:
 * -> On success:
 *    True
 * -> On failure:
 *    False
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
bool recvOkay(int                   sock,
              std::vector<uint8_t>& buffer,
              const uint8_t         expected_code);


// could also write another function here to combine the sendMsg and get response into one since it happens so much
// should prob push these back a layer into .internal/something.hpp if so

} //dfd
