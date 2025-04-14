#pragma once

#include "networking/messageFormatting.hpp"
#include "sourceInfo.hpp"

#include <string>
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptIndex
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to a server, submit an index request, and parse the
 *    response. If successful, adds the file to the provided vector for tracking
 *    what's currently indexed. If the server doesn't connect fast enough, or
 *    respond fast enough, returns with an error.
 *
 * Takes:
 * -> file_path:
 *    The path to the file to index. If the file is empty but exists, returns an
 *    error.
 * -> indexed_files:
 *    The vector to add this entry to for external use.
 * -> server:
 *    The server to attempt to connect to. If either the IP or port aren't
 *    present, returns with an error.
 * -> connection_timeout:
 *    The timeout for how long to wait while connecting to the server.
 * -> response_timeout:
 *    The timeout for how long to wait after connecting to the server whilst
 *    waiting for a reply.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptIndex(const  FileId&     file,
                 const  SourceInfo& server,
                 struct timeval     connection_timeout,
                 struct timeval     response_timeout);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptDrop
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to a server, submit a drop request, and parse the
 *    response. If successful, removes the file from the indexed_files vector for
 *    tracking what's currently indexed (and what is no longer indexed via this
 *    function). If the server doesn't connect fast enough, or respond fast
 *    enough, returns with an error.
 *
 * Takes:
 * -> file_path:
 *    The path to the file to drop. NOTE: this file should already be present
 *    in indexed_files, or an error will occur.
 * -> indexed_files:
 *    The vector to remove this entry from for external use.
 * -> server:
 *    The server to attempt to connect to. If either the IP or port aren't
 *    present, returns with an error.
 * -> connection_timeout:
 *    The timeout for how long to wait while connecting to the server.
 * -> response_timeout:
 *    The timeout for how long to wait after connecting to the server whilst
 *    waiting for a reply.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptDrop(const  IndexUuidPair& file,
                const  SourceInfo&    server,
                struct timeval        connection_timeout,
                struct timeval        response_timeout);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptSourceRetrieval
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to a server, submit a sourceList request, and parse
 *    the response into a (potentially empty) list of a sources. A successful
 *    return is still possible with an empty source list, and the return code
 *    will reflect as such. If the server doesn't connect fast enough, or
 *    respond fast enough, returns with an error.
 *
 * Takes:
 * -> file_uuid:
 *    The UUID of the file to get the source list for. 
 * -> dest:
 *    The vector to store the retrieved source list inside of. This vector is
 *    cleared during this process.
 * -> server:
 *    The server to attempt to connect to. If either the IP or port aren't
 *    present, returns with an error.
 * -> connection_timeout:
 *    The timeout for how long to wait while connecting to the server.
 * -> response_timeout:
 *    The timeout for how long to wait after connecting to the server whilst
 *    waiting for a reply.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptSourceRetrieval(const uint64_t           file_uuid,
                           std::vector<SourceInfo>& dest,
                           const  SourceInfo&       server,
                           struct timeval           connection_timeout,
                           struct timeval           response_timeout);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptServerUpdate
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to a server, submit a CLIENT_REG request, and parse
 *    the response.
 *
 * Takes:
 * -> dest:
 *    The vector of SourceInfo to store the result in.
 * -> server:
 *    The server to attempt to connect to. If either the IP or port aren't
 *    present, returns with an error.
 * -> connection_timeout:
 *    The timeout for how long to wait while connecting to the server.
 * -> response_timeout:
 *    The timeout for how long to wait after connecting to the server whilst
 *    waiting for a reply.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptServerUpdate(std::vector<SourceInfo>& dest,
                        const  SourceInfo&       server,
                        struct timeval           connection_timeout,
                        struct timeval           response_timeout);

} //dfd
