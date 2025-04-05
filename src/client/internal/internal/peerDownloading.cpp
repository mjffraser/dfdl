#include <optional>
#include <thread>
#include <iostream>

#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include "networking/fileParsing.hpp"
#include "client/internal/internal/peerDownloading.hpp"
#include "client/internal/internal/clientNetworking.hpp"

namespace dfd
{
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
                        std::condition_variable &chunk_ready)
    {
        while (true)
        {
            // Step 1: Get next chunk index
            size_t chunk_index;
            {
                std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
                if (remaining_chunks.empty())
                {
                    return; // Nothing left to download
                }
                chunk_index = remaining_chunks.front();
                remaining_chunks.pop();
            }

            // Step 2: Select a peer and mark as in-use
            std::optional<size_t> peer_index;
            {
                std::lock_guard<std::mutex> lock(source_mtx);
                for (size_t i = 0; i < sources.size(); ++i)
                {
                    if (source_stats[i])
                    {
                        source_stats[i] = false; // mark as in-use
                        peer_index = i;
                        break;
                    }
                }
            }

            if (!peer_index.has_value())
            {
                // Couldn’t find a free peer — push chunk back and retry later
                {
                    std::lock_guard<std::mutex> lock(remaining_chunks_mtx);
                    remaining_chunks.push(chunk_index);
                }
                std::this_thread::yield(); // let another thread try
                continue;
            }

            const SourceInfo& peer = sources[peer_index.value()];

            // Step 3: Connect to the peer
            struct timeval tv;
            tv.tv_sec = PEER_CONNECT_TIMEOUT_SEC;
            tv.tv_usec = PEER_CONNECT_TIMEOUT_USEC;

            int sock = connectToSource(peer, tv);
            if (sock < 0)
            {
                std::lock_guard<std::mutex> lock(bad_peer_mtx);
                bad_peers.push_back(peer);
                continue;
            }

            // Step 4: Send download request
            std::vector<uint8_t> download_req = createDownloadInit(file_uuid, std::nullopt);
            if (!sendOkay(sock, download_req)) {
                // How to handle?
                continue;
            }

            std::vector<uint8_t> download_ack;
            if (!recvOkay(sock, download_ack, DOWNLOAD_CONFIRM)) {
                // How to handle?
                continue;
            }
            auto f_info = parseDownloadConfirm(download_ack);
            uint64_t f_size = f_info.first;
            std::string f_name = f_info.second;

            // Step 5: Try to receive chunk
            std::vector<uint8_t> chunk_req = createChunkRequest(chunk_index);
            if (!sendOkay(sock, chunk_req)) {
                // How to handle?
                continue;
            }

            std::vector<uint8_t> chunk_data;
            if (!recvOkay(sock, chunk_data, DATA_CHUNK)) {
                // How to handle?
                continue;
            }

            DataChunk dc = parseDataChunk(chunk_data);
            unpackFileChunk(f_name, dc.second, dc.second.size(), chunk_index);
        
            // Step 6: Notify main thread of done chunk
            std::cout << "Downloaded chunk " << chunk_index << std::endl;
            {
                std::lock_guard<std::mutex> lock(done_chunks_mtx);
                done_chunks.push(chunk_index);
            }
            chunk_ready.notify_one();

            closeSocket(sock);
        }
    }
}