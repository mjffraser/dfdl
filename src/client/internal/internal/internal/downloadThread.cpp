#include <optional>
#include <thread>
#include <iostream>

#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include "networking/fileParsing.hpp"
#include "client/internal/internal/peerDownloading.hpp"
#include "client/internal/internal/clientNetworking.hpp"

namespace dfd {
void downloadThread(const uint64_t file_uuid,
                    const std::vector<SourceInfo> &sources,
                    std::vector<bool> &source_stats,
                    std::mutex &source_mtx,
                    std::vector<SourceInfo> &bad_peers,
                    std::mutex &bad_peer_mtx,
                    std::queue<size_t> &remaining_chunks,
                    std::mutex &remaining_chunks_mtx,
                    std::queue<size_t> &done_chunks,
                    std::mutex &done_chunks_mtx,
                    std::condition_variable &chunk_ready) {
    // Select a peer and mark as in-use
    std::optional<size_t> peer_index;
    {
        std::lock_guard<std::mutex> lock(source_mtx);
        for (size_t i = 0; i < sources.size(); ++i) {
            if (source_stats[i]) {
                source_stats[i] = false; // mark as in-use
                peer_index = i;
                break;
            }
        }
    }

    if (!peer_index.has_value()) {
        // No available peer, exit this thread
        return;
    }

    const SourceInfo &peer = sources[peer_index.value()];

    // Connect to the peer
    struct timeval tv;
    tv.tv_sec = PEER_CONNECT_TIMEOUT_SEC;
    tv.tv_usec = PEER_CONNECT_TIMEOUT_USEC;

    int sock = connectToSource(peer, tv);
    if (sock < 0) {
        std::lock_guard<std::mutex> lock(bad_peer_mtx);
        bad_peers.push_back(peer);
        return;
    }


    while (true) {
        if (bad_peers.size() == sources.size()) {
            // All peers are bad, exit thread
            return;
        }

        // Get next chunk index
        size_t chunk_index;
        {
            std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
            if (remaining_chunks.empty()) {
                return; // Nothing left to download
            }
            chunk_index = remaining_chunks.front();
            remaining_chunks.pop();
        }

        int download_status = downloadChunk(file_uuid, sock, chunk_index);
        if (download_status == RECV_FAIL) {
            std::lock_guard<std::mutex> lock(bad_peer_mtx);
            bad_peers.push_back(peer);
            continue;
        } else if (download_status == SEND_FAIL) {
            std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
            remaining_chunks.push(chunk_index);
            continue;
        }

        // Notify main thread of done chunk
        std::cout << "Downloaded chunk " << chunk_index << std::endl;
        {
            std::lock_guard<std::mutex> lock(done_chunks_mtx);
            done_chunks.push(chunk_index);
        }
        chunk_ready.notify_one();

        closeSocket(sock);
    }
}

int downloadChunk(const uint64_t file_uuid,
                  const int sock,
                  const size_t chunk_index,
                  std::optional<std::pair<uint64_t, std::string>>* f_info) {
    // Send download request
    std::vector<uint8_t> download_req = createDownloadInit(file_uuid, std::nullopt);
    if (!sendOkay(sock, download_req)) {
        return SEND_FAIL;
    }

    std::vector<uint8_t> download_ack;
    if (!recvOkay(sock, download_ack, DOWNLOAD_CONFIRM)) {
        return RECV_FAIL;
    }

    auto file_info = parseDownloadConfirm(download_ack);
    auto f_name = file_info.second;
    if (f_info) {
        *f_info = file_info;
    }

    // Try to receive chunk
    std::vector<uint8_t> chunk_req = createChunkRequest(chunk_index);
    if (!sendOkay(sock, chunk_req)) {
        return SEND_FAIL;
    }

    std::vector<uint8_t> chunk_data;
    if (!recvOkay(sock, chunk_data, DATA_CHUNK)) {
        return RECV_FAIL;
    }

    DataChunk dc = parseDataChunk(chunk_data);
    unpackFileChunk(f_name, dc.second, dc.second.size(), chunk_index);

    return SUCCESS;
}

} //dfd
