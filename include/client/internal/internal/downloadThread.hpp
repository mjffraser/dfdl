#pragma once

#include "sourceInfo.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * downloadThread
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Designed to be opened as a thread, which will connect to peers in the
 *    sources list to download chunks of the file. 
 *    -> To select a peer, a thread will aquire a lock on source_mutex, select a
 *       free source (indicated by a status of True in the source_stats list),
 *       will mark that source as used (setting the entry in source_stats
 *       False), then release the lock for other threads to select a peer.
 *    -> To record a bad peer, a thread will aquire a lock on bad_peer_mtx,
 *       add the SourceInfo of the faulty peer, and release the lock.
 *    -> To get the next chunk needed, a thread will aquire a lock on
 *       remaining_chunks_mtx, pop the next chunk off the queue, and release the
 *       lock.
 *    -> To report successful storage of the chunk to disk for compiling, a
 *       thread will aquire a lock on done_chunks_mtx, push the chunk index onto
 *       the queue, release the lock, and notify the chunk_ready CV. 
 *
 * Takes:
 * -> file_uuid:
 *    The UUID of the file to download chunks for.
 * -> sources:
 *    A list of SourceInfo's for the various peers returned by the server.
 * -> source_stats:
 *    A list of booleans that correspond 1:1 to the SourceInfo's in sources.
 *    True corresponds to a free peer, False corresponds to a peer in-use by
 *    another thread. Peers should not be set from false -> true.
 * -> stat_mtx:
 *    The mutex to lock while utilizing the above two vectors.
 * -> bad_peers:
 *    A vector of SourceInfo's of bad peers that failed connections/downloads.
 * -> bad_peers_mtx:
 *    The mutex to lock while appending to the above vectors.
 * -> remaining_chunks:
 *    A queue of remaining chunk indexes to request.
 * -> remaining_chunks_mtx:
 *    A mutex to lock while popping from the above queue.
 * -> done_chunks:
 *    A queue of chunk indexes that have been successfully written to disk so
 *    they can be compiled by the main thread.
 * -> done_chunks_mtx:
 *    A mutex to lock while pushing to the above queue.
 * -> chunk_ready:
 *    A condition variable to notify when a chunk is pushed to the done_chunks
 *    queue.
 * -> connection_timeout:
 *    How long to wait when attempting a peer connection.
 * -> response_timeout:
 *    How long to wait when waiting for a reply from the peer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void downloadThread(const uint64_t                 f_uuid,
                    const std::vector<SourceInfo>& sources,
                    std::vector<bool>&             source_stats,
                    std::mutex&                    stat_mtx,
                    std::vector<SourceInfo>&       bad_peers,
                    std::mutex&                    bad_peers_mtx,
                    std::queue<size_t>&            remaining_chunks,
                    std::mutex&                    remaining_chunks_mtx,
                    std::queue<size_t>&            done_chunks,
                    std::mutex&                    done_chunks_mtx,
                    std::condition_variable&       chunk_ready,
                    struct timeval                 connection_timeout,
                    struct timeval                 response_timeout);

}
