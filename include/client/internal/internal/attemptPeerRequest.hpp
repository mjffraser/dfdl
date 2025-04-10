#pragma once

#include "sourceInfo.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * selectPeerSource
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Given the f_stat vector of all currently in-use peers, finds the first
 *    available (true). Sets that peer false and returns its index for use.
 *
 * Takes:
 * -> f_stats:
 *    See desc.
 *
 * Returns:
 * -> On success:
 *    The index of the peer (0 indexed for direct vector access)
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int selectPeerSource(std::vector<bool>& f_stats);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptInitialChunkDownload
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attempts to connect to, and download the first chunk of a file from a
 *    provided peer. If successful, f_name and f_size will be set with the
 *    information obtained in the DOWNLOAD_CONFIRM message so calculations for
 *    how many threads to open for the remaining chunks can occur, as well as
 *    file_ptr for the actual file to compile chunks into.
 *
 * Takes:
 * -> f_uuid:
 *    The uuid of the file to request.
 * -> f_name:
 *    A reference to a std::string to store the file name on success.
 * -> f_size:
 *    A reference to a uint64_t to store the file size on success.
 * -> file_ptr:
 *    A reference to a std::unique_ptr<std::ofstream> that, on success, will be
 *    set to the open file and, on failure, will be left alone.
 * -> peer:
 *    The peer to attempt connecting to.
 * -> connection_timeout:
 *    The timeout for connecting to the peer.
 * -> response_timeout:
 *    The timeout for the peers response.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptInitialChunkDownload(const  uint64_t                        f_uuid,       
                                       std::string&                    f_name,
                                       uint64_t&                       f_size,
                                       std::unique_ptr<std::ofstream>& file_ptr,
                                const  SourceInfo&                     peer,
                                struct timeval                         connection_timeout,
                                struct timeval                         response_timeout);

} //dfd
