#pragma once

#include "sourceInfo.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>
namespace dfd {

// PEER CONNECT TIMEOUT
#define PEER_CONNECT_TIMEOUT_SEC 2 // seconds
#define PEER_CONNECT_TIMEOUT_USEC 0 // microseconds

// MAX THREADS TO OPEN
#define SEED_THREAD_LIMIT 5

// CHUNK DOWNLOAD RETURN CODES
#define SUCCESS 0
#define SEND_FAIL 1
#define RECV_FAIL 2

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
 * -> source_mtx:
 *    The mutex to lock while utilizing the above two vectors.
 * -> bad_peers:
 *    A vector of SourceInfo's of bad peers that failed connections/downloads.
 * -> bad_peer_mtx:
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
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void downloadThread(const uint64_t                 file_uuid,
                    const std::vector<SourceInfo>& sources,
                    std::vector<bool>&             source_stats,
                    std::mutex&                    source_mtx,
                    std::vector<SourceInfo>&       bad_peers,
                    std::mutex&                    bad_peer_mtx,
                    std::queue<size_t>&            remaining_chunks,
                    std::mutex&                    remaining_chunks_mtx,
                    std::queue<size_t>&            done_chunks,
                    std::mutex&                    done_chunks_mtx,
                    std::condition_variable&       chunk_ready);
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * downloadChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Downloads a chunk from a peer. This function is called by the main thread
 *    when it tries to download the first chunk, and also from the worker threads
 *    when they are assigned a chunk to download.
 *
 * Takes:
 * -> file_uuid:
 *    The UUID of the file to download chunk for.
 * -> sock:
 *    The socket to connect to the peer with.
 * -> chunk_index:
 *    The index of the chunk to download.
 * -> f_size:
 *    Optional pointer to file size, which is populated by the function and may be 
 *    used to calculate the number of chunks.
 * 
 * Returns:
 * -> On success:
 *    CHUNK_DOWNLOADED
 * -> On failure:
 *    SEND_FAIL or RECV_FAIL, depending on which failed. A SEND_FAIL can probably be
 *    ignored, but a RECV_FAIL indicates the peer is bad and should be removed from
 *    the sources list.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int downloadChunk(const uint64_t                   file_uuid,
                  const int                        sock,
                  const size_t                     chunk_index,
                  std::optional<std::pair<uint64_t, 
                  std::string>>*                   f_info = nullptr);

}
