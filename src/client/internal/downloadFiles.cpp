#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <optional>
#include <iostream>
#include <algorithm>

#include "client/internal/downloadFiles.hpp"
#include "client/internal/internal/peerDownloading.hpp"
#include "client/internal/internal/clientNetworking.hpp"
#include "networking/fileParsing.hpp"
namespace dfd
{
    int attemptFileDownload(const uint64_t file_uuid,
                            std::vector<SourceInfo> &sources,
                            std::vector<SourceInfo> &bad_sources)
    {
        bad_sources.clear();

        std::vector<bool> source_stats(sources.size(), true);
        std::mutex source_mtx;
        std::vector<SourceInfo> bad_peers;
        std::mutex bad_peer_mtx;

        std::queue<size_t> remaining_chunks;
        std::mutex remaining_chunks_mtx;

        std::queue<size_t> done_chunks;
        std::mutex done_chunks_mtx;
        std::condition_variable chunk_ready;

        int sock;
        for (SourceInfo &peer : sources)
        {
            // Connect to the peer
            struct timeval tv;
            tv.tv_sec = PEER_CONNECT_TIMEOUT_SEC;
            tv.tv_usec = PEER_CONNECT_TIMEOUT_USEC;

            sock = connectToSource(peer, tv);
            if (sock < 0)
            {
                std::lock_guard<std::mutex> lock(bad_peer_mtx);
                bad_peers.push_back(peer);
            }
            else
            {
                break;
            }
        }
        if (sock < 0)
        {
            // All peers are bad, exit thread
            std::cerr << "Couldn't connect to any peer to get first chunk." << std::endl;
            return EXIT_FAILURE;
        }

        std::optional<std::pair<uint64_t, std::string>> f_info;
        int download_status = downloadChunk(file_uuid, sock, 0, &f_info);
        uint64_t f_size = f_info ? f_info->first : 0;
    std::string f_name = "below line is error?";
        // std::string f_name = f_info ? f_info->second : '\0';
        // TO-DO: handle cases
        // need to retry other peers if failure, otherwise we push 0 to the done_chunks queue
        {
            std::lock_guard<std::mutex> lock(done_chunks_mtx);
            done_chunks.push(0);
        }

        size_t total_chunks = fileChunks(static_cast<size_t>(f_size)).value_or(0);
        if (total_chunks == 0)
        {
            std::cerr << "Failed to get file chunks." << std::endl;
            return EXIT_FAILURE;
        }
        for (size_t i = 1; i < total_chunks; ++i)
        {
            remaining_chunks.push(i);
        }

        if (f_name.empty())
        {
            std::cerr << "Failed to get file name." << std::endl;
            return EXIT_FAILURE;
        }
        std::unique_ptr<std::ofstream> file = openFile(f_name);
        if (!file)
        {
            std::cerr << "Couldn't start writing file." << std::endl;
            return EXIT_FAILURE;
        }

        // we want to select a number of concurrent download threads to use
        // we select the minimum of:
        //  -> available peers
        //  -> hardware thread limitations
        //  -> number of chunks that still need downloading (opening 8 threads for 3 chunks is a waste)
        //  -> 5 threads
        size_t num_threads = std::min(sources.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
        num_threads = std::min(num_threads, remaining_chunks.size());
        num_threads = std::min(num_threads, static_cast<size_t>(SEED_THREAD_LIMIT));

        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i)
        {
            threads.emplace_back(downloadThread,
                                 file_uuid,
                                 std::ref(sources),
                                 std::ref(source_stats),
                                 std::ref(source_mtx),
                                 std::ref(bad_peers),
                                 std::ref(bad_peer_mtx),
                                 std::ref(remaining_chunks),
                                 std::ref(remaining_chunks_mtx),
                                 std::ref(done_chunks),
                                 std::ref(done_chunks_mtx),
                                 std::ref(chunk_ready));
        }

        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(done_chunks_mtx);
                chunk_ready.wait(lock, [&]
                                 { return !done_chunks.empty(); });
                while (!done_chunks.empty())
                {
                    size_t c = done_chunks.front();
                    done_chunks.pop();
                    assembleChunk(file.get(), f_name, c);
                }

                {
                    std::unique_lock<std::mutex> lock(remaining_chunks_mtx);
                    if (remaining_chunks.empty())
                        break;
                }
            }
        }

        for (auto &t : threads)
        {
            if (t.joinable())
                t.join();
        }

        std::lock_guard<std::mutex> lock(done_chunks_mtx);
        if (done_chunks.size() == total_chunks)
        {
            // Success! Clean up bad peers
            {
                std::lock_guard<std::mutex> bad_lock(bad_peer_mtx);
                bad_sources = bad_peers;
            }

            // Remove bad peers from sources
            sources.erase(std::remove_if(sources.begin(), sources.end(),
                                         [&bad_sources](const SourceInfo &peer)
                                         {
                                             return std::find(bad_sources.begin(), bad_sources.end(), peer) != bad_sources.end();
                                         }),
                          sources.end());

            while (!done_chunks.empty())
            {
                size_t c = done_chunks.front();
                done_chunks.pop();
                assembleChunk(file.get(), f_name, c);
            }

            saveFile(std::move(file));
            std::cout << "Downloaded file." << std::endl;
            return EXIT_SUCCESS;
        }
        else
        {
            // Failure — empty sources and move all to bad_sources
            bad_sources = std::move(sources);
            sources.clear();
            return EXIT_FAILURE;
        }
    }
}
