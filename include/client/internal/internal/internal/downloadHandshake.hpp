#pragma once

#include <cstdint>
#include <string>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptDownloadHandshake
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes an already connected socket to a peer indexing a file and sends a
 *    DOWNLOAD_INIT message. Waits for a DOWNLOAD_CONFIRM message to obtain the
 *    file name and size. If any error occurs, the socket is CLOSED, and an
 *    error is returned.
 *
 * Takes:
 * -> connected_sock:
 *    The peer to do the handshake with.
 * -> f_uuid:
 *    The uuid of the file to request.
 * -> f_name:
 *    A reference to a std::string to, on success, put the file name into.
 * -> f_size:
 *    A reference to a uint64_t to, on success, put the file size into.
 * -> response_timeout:
 *    The timeout for how long to wait for the DOWNLOAD_CONFIRM message.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptDownloadHandshake(int            connected_sock,
                             const uint64_t f_uuid,
                             std::string&   f_name,
                             uint64_t&      f_size,
                             struct timeval response_timeout);

} //dfd
