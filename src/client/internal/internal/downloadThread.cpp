#include "client/internal/internal/downloadThread.hpp"
#include "client/internal/internal/attemptPeerRequest.hpp"
#include "client/internal/internal/internal/clientNetworking.hpp"
#include "client/internal/internal/internal/downloadHandshake.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include "networking/fileParsing.hpp"

#include <optional>
#include <thread>
#include <iostream>

namespace dfd {



/* 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * selectPeerThreaded
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Calls selectPeerSource after aquiring a lock on the source_stats mutex. 
 * 
 * Takes:
 * -> source_stats:
 *    Arg for selectPeerSource()
 * -> stat_mtx:
 *    The mutex to lock.
 * 
 * Returns:
 * -> On success:
 *    The success condition from selectPeerSource (INDEX OF SLECTED PEER)
 * -> On failure:
 *    The failure condition from selectPeerSource (-1)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int selectPeerThreaded(std::vector<bool>& source_stats, std::mutex& stat_mtx) {
    std::lock_guard<std::mutex> lock(stat_mtx);
    return selectPeerSource(source_stats);
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * addBadPeer
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Aquires a lock on the bad_peers mutex, then adds the bad peer to the list.
 *
 * Takes:
 * -> bad_peer:
 *    The peer to add.
 * -> bad_peers:
 *    The list.
 * -> bad_peers_mtx:
 *    The mutex to lock.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void addBadPeer(const SourceInfo&              bad_peer,
                      std::vector<SourceInfo>& bad_peers,
                      std::mutex&              bad_peers_mtx) {
    std::lock_guard<std::mutex> lock(bad_peers_mtx);
    bad_peers.push_back(bad_peer);
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * getNextChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Aquires a lock on the remaining_chunks mutex, checks the queue for any
 *    remaining chunks, pops the next in the queue if it exists, then returns
 *    it. If no chunks are left 0 is returned as the main thread downloads the
 *    0th chunk when opening the file. There's no circumstance where it should
 *    be present in the remaining_chunks queue.
 *
 * Takes:
 * -> remaining_chunks:
 *    The queue of chunks.
 * -> remaining_chunks_mtx:
 *    The mutex to lock.
 *
 * Returns:
 * -> On success:
 *    The chunk to download.
 * -> On failure:
 *    0
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
size_t getNextChunk(std::queue<size_t>& remaining_chunks,
                    std::mutex&         remaining_chunks_mtx) {
    std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
    if (remaining_chunks.empty())
        return 0;
    size_t chunk = remaining_chunks.front();
    remaining_chunks.pop();
    return chunk;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * downloadChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Downloads a chunk from a peer. 
 *
 * Takes:
 * -> sock:
 *    The connected, post-handshake peer socket. 
 * -> chunk_index:
 *    The index of the chunk to download.
 * -> f_name:
 *    The name of the file for unpackFileChunk()
 * -> response_timeout:
 *    How long to wait for a reply.
 * 
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS 
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int downloadChunk(int                sock,
                  const size_t       chunk_index,
                  const std::string& f_name,
                  struct timeval     response_timeout) {
    //Try to receive chunk
    std::vector<uint8_t> chunk_req = createChunkRequest(chunk_index);
    std::vector<uint8_t> chunk_data;
    if (EXIT_FAILURE == sendAndRecv(sock,
                                    chunk_req,
                                    chunk_data,
                                    DATA_CHUNK,
                                    response_timeout)) {
        return EXIT_FAILURE;
    }

    //store received datachunk
    DataChunk dc = parseDataChunk(chunk_data);
    unpackFileChunk(f_name, dc.second, dc.second.size(), chunk_index);

    return EXIT_SUCCESS;
}

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
                    struct timeval                 response_timeout) {
    size_t chunks_obtained = 0;
    int    peer_index;
    while ((peer_index = selectPeerThreaded(source_stats, stat_mtx)) >= 0) {
        //select peer
        const SourceInfo& selected_peer = sources[peer_index];
        std::cout << "SELECTING " << selected_peer.ip_addr << ":" << selected_peer.port << std::endl;

        //attempt connection
        int sock = connectToSource(selected_peer, connection_timeout); 
        if (sock < 0) {
            addBadPeer(selected_peer, bad_peers, bad_peers_mtx);
            continue;
        }

        //do handshake with peer
        std::string f_name;
        uint64_t    f_size;
        if (EXIT_FAILURE == attemptDownloadHandshake(sock,
                                                     f_uuid,
                                                     f_name,
                                                     f_size,
                                                     response_timeout)) {
            addBadPeer(selected_peer, bad_peers, bad_peers_mtx);
            return; //socket closed by attemptDownloadHandshake
        }

        //chunk request loop, while chunks are in the queue we:
        size_t chunk_index;
        while ((chunk_index = getNextChunk(remaining_chunks, remaining_chunks_mtx)) != 0) {
            // std::cout << "next:" << chunk_index << " from " << selected_peer.port << std::endl; 
            if (EXIT_SUCCESS != downloadChunk(sock,
                                              chunk_index,
                                              f_name,
                                              response_timeout)) {
                break;
            }

            {
                //record downloaded chunk
                std::unique_lock<std::mutex> lock(done_chunks_mtx);
                done_chunks.push(chunk_index);
            }
            chunk_ready.notify_one();
        }

        //done with this peer
        sendOkay(sock, {FINISH_DOWNLOAD});
        closeSocket(sock);

        if (chunk_index == 0) {
            //nothing left to do
            std::lock_guard<std::mutex> lock(stat_mtx);
            source_stats[peer_index] = true; //this peer is fine
            return;
        } else {
            //if this peer isn't responding
            if (chunks_obtained < 1)
                addBadPeer(selected_peer, bad_peers, bad_peers_mtx);
            std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
            remaining_chunks.push(chunk_index);
        }
    }
}


} //dfd
