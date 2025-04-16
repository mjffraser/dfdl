#include "client/internal/requests.hpp"
#include "client/internal/internal/attemptServerRequest.hpp"
#include "client/internal/internal/attemptPeerRequest.hpp"
#include "client/internal/internal/downloadThread.hpp"
#include "networking/fileParsing.hpp"
#include "networking/messageFormatting.hpp"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>

namespace dfd {

static bool           timeout_init = false;
static struct timeval connection_timeout;
static struct timeval response_timeout;
static struct timeval update_timeout;

void init_timeouts() {
    //CONNECTION TIMEOUT: 0.5s
    connection_timeout.tv_sec  = 0;
    connection_timeout.tv_usec = 500000;

    //RESPONSE TIMEOUT: 2s
    response_timeout.tv_sec  = 2;
    response_timeout.tv_usec = 0;

    //UPDATE TIMEOUT: 1s
    update_timeout.tv_sec  = 1;
    update_timeout.tv_usec = 0;

    timeout_init = true;
}

int updateServerList(std::vector<SourceInfo>& server_list)
{
    const std::vector<SourceInfo> snapshot = server_list;

    for (const SourceInfo& srv : snapshot)
    {
        std::vector<SourceInfo> new_list;

        if (attemptServerUpdate(new_list,
                                srv,
                                connection_timeout,
                                response_timeout) == EXIT_SUCCESS)
        {
            new_list.push_back(srv);

            server_list.swap(new_list);
            return EXIT_SUCCESS;
        }
    }

    return EXIT_FAILURE;
}

std::optional<FileId> parseFile(const SourceInfo&  my_listener,
                                const std::string& f_path) {
    if (!std::filesystem::exists(f_path)) {
        std::cerr << "[err] file cannot be found." << std::endl;
        return std::nullopt;
    }

    uint64_t f_uuid = sha256Hash(f_path);
    if (f_uuid == 0) {
        std::cerr << "[err] file uuid could not be computed." << std::endl;
        return std::nullopt;
    }

    auto f_size_opt = fileSize(f_path);
    if (!f_size_opt) {
        std::cerr << "[err] failed to get file size." << std::endl;
        return std::nullopt;
    }

    return FileId(f_uuid, my_listener, f_size_opt.value());
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doAttempts
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Template function that executes one of:
 *    => attemptIndex
 *    => attemptDrop
 *    => attemptSourceRetrieval
 *    from attemptServerRequest.
 *
 *    This function should be provided all args up to but NOT including:
 *    => const SourceInfo& server
 *    => struct timeval connection_timeout
 *    => struct timeval response_timeout
 *
 *    An example call of this function is:
 *    doAttempts(server_list, doSourceRetrieval, file_uuid, dest);
 *
 *    This function exponentially increases the connection timeout, doubling it
 *    with each failed attempt. In total, three tries occur, and if all those
 *    fail the server list is refreshed. After that, three more tries occur on
 *    each server. Only if all 6n attempts fail does this function return a
 *    fail.
 *
 * Takes:
 * -> server_list:
 *    The most up-to-date list of servers on the network.
 * -> fn:
 *    The doXXX function to call.
 * -> fn_args:
 *    A variable number of args for that function.
 *
 * Returns:
 * -> On success:
 *    True
 * -> On failure:
 *    False
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
template <typename Func, typename... FuncArgs>
bool doAttempts(std::vector<SourceInfo>& server_list,
                Func                     fn,
                FuncArgs&&...            fn_args) {
    std::vector<SourceInfo> bad_servers;
    bool success = false;

    updateServerList(server_list);

    //try once with current server list, then update it and try one more time
    for (int i = 0; i < 2 && !success; ++i) {
        struct timeval conn_timeout = connection_timeout;
        //try three times, with increasing timeouts on each attempt
        for (int j = 0; j < 3 && !success; ++j) {
            conn_timeout.tv_sec = connection_timeout.tv_sec << j;
            for (const SourceInfo& server : server_list) {
                if (EXIT_SUCCESS == fn(std::forward<FuncArgs>(fn_args)...,
                                       server,
                                       conn_timeout,
                                       response_timeout)) {
                    std::cout << "GOT RESPONSE" << std::endl;
                    return true;
                } else {
                    if (i == 1 && j == 2) //final attempt and still no response
                        bad_servers.push_back(server);
                }
            }
        }
    }

    return success;
}

int doIndex(const SourceInfo&                      my_listener,
            const std::string&                     file_path,
                  std::map<uint64_t, std::string>& indexed_files,
                  std::mutex&                      indexed_files_mtx,
                  std::vector<SourceInfo>&         server_list) {
    if (!timeout_init) init_timeouts();

    auto file = parseFile(my_listener, file_path);
    if (!file)
        return EXIT_FAILURE;
    FileId& f_info = file.value();

    std::cout << "Indexing..." << std::endl;

    if (doAttempts(server_list, attemptIndex, f_info)) {
        std::unique_lock<std::mutex> lock(indexed_files_mtx);
        indexed_files[f_info.uuid] = std::filesystem::absolute(file_path);
        std::cout << "File: '" << f_info.uuid << "' is now indexed with the DFD network." << std::endl;
        return EXIT_SUCCESS;
    } else {
        std::cerr << "[err] Sorry, tried all known servers twice, and received no response from any." << std::endl;
        return EXIT_FAILURE;
    }
}

int doDrop(const SourceInfo&                       my_listener,
            const std::string&                     file_path,
                  std::map<uint64_t, std::string>& indexed_files,
                  std::mutex&                      indexed_files_mtx,
                  std::vector<SourceInfo>&         server_list) {
    if (!timeout_init) init_timeouts();

    auto file = parseFile(my_listener, file_path);
    if (!file)
        return EXIT_FAILURE;
    FileId& f_info = file.value();
    IndexUuidPair drop_pair = std::make_pair(f_info.uuid, f_info.indexer.peer_id);

    if (indexed_files.count(f_info.uuid) < 1) {
        std::cerr << "This file is not currently indexed." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Dropping..." << std::endl;

    if (doAttempts(server_list, attemptDrop, drop_pair)) {
        indexed_files[f_info.uuid].erase();
        std::cout << "File: '" << f_info.uuid << "' is now dropped from the DFD network." << std::endl;
        return EXIT_SUCCESS;
    } else {
        std::cerr << "[err] Sorry, tried all known servers twice, and received no response from any." << std::endl;
        return EXIT_FAILURE;
    }
}

int doDownload(const uint64_t                 f_uuid,
                     std::vector<SourceInfo>& server_list) {
    if (!timeout_init) init_timeouts();

    //connect to server, grab sources
    std::cout << "Sourcing file..." << std::endl;

    ///////////////////////////////////////////////////////////////////////////
    ///STAGE 1: GET THE BEST SOURCE LIST

    std::vector<SourceInfo> f_sources;
    if (!doAttempts(server_list,
                    attemptSourceRetrieval,
                    f_uuid,
                    f_sources)) {
        std::cerr << "[err] Sorry, tried all known servers twice, and received no response from any." << std::endl;
        std::cerr << "[err] Could not find any peers." << std::endl;
        return EXIT_FAILURE;
    }

    //TODO: more fault tolerance here with server syncing, try other servers dynamically.
    if (f_sources.empty()) {
        std::cerr << "[err] Server responded, but no sources available. Sorry." << std::endl;
        return EXIT_FAILURE;
    }

    //aquire file info & the first chunk
    std::vector<bool> f_stats(f_sources.size(), true);
    int peer_ind;
    std::string f_name;
    uint64_t    f_size;
    std::unique_ptr<std::ofstream> file_out = nullptr;
    while ((peer_ind = selectPeerSource(f_stats)) >= 0) {
        const SourceInfo& server = f_sources[peer_ind];
        if (EXIT_SUCCESS == attemptInitialChunkDownload(f_uuid,
                                                        f_name,
                                                        f_size,
                                                        file_out,
                                                        server,
                                                        connection_timeout,
                                                        response_timeout)) {
            f_stats[peer_ind] = true; //reset, a download thread can use this peer
            break;
        }
        //otherwise, move on to next peer in the list
    }

    ///END STAGE 1: WE LOCK IN THE PEER LIST WE'RE USING PAST THIS POINT
    ///////////////////////////////////////////////////////////////////////////
    ///STAGE 2: DOWNLOAD REMAINING CHUNKS OF FILE

    if (file_out == nullptr) {
        if (f_name.empty()) //if not empty, we already have the file in our download dir, and errored due to that
            std::cerr << "[err] Exhausted peer list before a peer responded. Please try again later." << std::endl;
        return EXIT_FAILURE;
    }

    auto chunks_in_file_opt = fileChunks(f_size);
    if (!chunks_in_file_opt) {
        std::cerr << "[err] Received erroneous file size." << std::endl;
        return EXIT_FAILURE;
    }

    size_t f_chunks = chunks_in_file_opt.value();
    if (f_chunks > 1) {
        std::queue<size_t> remaining_chunks;
        std::queue<size_t> done_chunks;
        std::vector<SourceInfo> bad_peers;

        std::mutex remaining_chunks_mtx;
        std::mutex done_chunks_mtx;
        std::mutex f_stat_mtx;
        std::mutex bad_peers_mtx;
        std::condition_variable chunk_ready;

        //build chunk list to download
        for (size_t i = 1; i < f_chunks; ++i) remaining_chunks.push(i);

        //we want to select a number of concurrent download threads to use
        //we select the minimum of:
        // -> available peers
        // -> hardware thread limitations
        // -> number of chunks that still need downloading (opening 8 threads for 3 chunks is a waste)
        // -> 5 threads
        size_t num_threads = std::min(f_sources.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
        num_threads        = std::min(num_threads,      remaining_chunks.size());
        num_threads        = std::min(num_threads,      static_cast<size_t>(5));

        std::vector<std::thread> workers; workers.resize(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers[i] = std::thread(downloadThread,
                                     f_uuid,
                                     std::cref(f_sources),
                                     std::ref(f_stats),
                                     std::ref(f_stat_mtx),
                                     std::ref(bad_peers),
                                     std::ref(bad_peers_mtx),
                                     std::ref(remaining_chunks),
                                     std::ref(remaining_chunks_mtx),
                                     std::ref(done_chunks),
                                     std::ref(done_chunks_mtx),
                                     std::ref(chunk_ready),
                                     std::ref(connection_timeout),
                                     std::ref(response_timeout));
        }

        bool   timed_out      = false;
        size_t chunks_written = 0;

        //construct chunks
        while (true) {
            std::stringstream download_stream; 
            download_stream << "[";
            double chunk_percentage = (double)chunks_written / (double)f_chunks;
            double thresh = 80 * chunk_percentage;
            for (int i = 0; i < 80; i++) {
                if (i < thresh) download_stream << "#";
                else download_stream << "-";
            }
            download_stream << "] " << (std::floor(chunk_percentage*100)) << "%\r";
            std::cout << download_stream.str() << std::flush;

            std::unique_lock<std::mutex> dc_lock(done_chunks_mtx);
            bool notified = chunk_ready.wait_for(dc_lock, std::chrono::seconds(10), [&] {
                return !done_chunks.empty();
            });

            if (!notified) {
                //all threads gave up
                timed_out = true;
                break;
            }

            while (!done_chunks.empty()) {
                size_t c = done_chunks.front(); done_chunks.pop();
                assembleChunk(file_out.get(), f_name, c);
                chunks_written++;
            }

            {
                std::unique_lock<std::mutex> rc_lock(remaining_chunks_mtx);
                if (remaining_chunks.empty()) break;
            }
        }

        //join all threads and clean up
        for (auto& w : workers) w.join();
        if (timed_out) {
            std::cerr << "[err] All peers have dropped out mid-download. Cannot continue, sorry." << std::endl;
            return EXIT_FAILURE;
        }

        //any chunks that haven't been written yet
        while (!done_chunks.empty()) {
            size_t c = done_chunks.front(); done_chunks.pop();
            assembleChunk(file_out.get(), f_name, c);
            chunks_written++;
        }

        std::cout << "[################################################################################] 100%";
        std::cout << std::endl;

        if (chunks_written != f_chunks-1) { //0th is already written
            std::cerr << "[err] Some chunks were corrupted and no peers remain to re-request from. Sorry." << std::endl;
            return EXIT_FAILURE;
        }

        //file ptr is still cleaned up when deallocated, even if saveFile not called.
    }

    saveFile(std::move(file_out));
    std::cout << "Downloaded file." << std::endl;
    return EXIT_SUCCESS;
}

} //dfd
