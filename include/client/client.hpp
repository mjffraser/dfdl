#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <cstdint>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "sourceInfo.hpp"         // for SourceInfo
#include "networking/messageFormatting.hpp"  // for FileId, IndexUuidPair, etc.

namespace dfd
{

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * P2PClient
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Manages a peer-to-peer client that connects to an indexing server
 *    and shares/downloads files among peers.
 *
 *    All outbound/inbound messages to other peers or the indexing server
 *    are created/parsed via functions in messageFormatting.hpp. This avoids
 *    manual string-based protocols.
 *
 * Member Variables:
 * -> server_ip_:
 *    The IP address of the index server.
 * -> server_port_:
 *    The port of the index server.
 * -> is_running_:
 *    Indicates if the client is in the main loop or shutting down.
 * -> shared_files_:
 *    A map from generated 64-bit file ID -> local filename (for files we share).
 * -> share_mutex_:
 *    Guards shared_files_ from concurrent read/write.
 * -> my_listen_sock:
 *    Socket used to listen for incoming peer connections.
 * -> my_listen_thread:
 *    Background thread that accepts incoming connections on my_listen_sock.
 *
 * Constructor:
 * -> Takes:
 *    -> server_ip:
 *       The index server IP.
 *    -> server_port:
 *       The index server port.
 *
 * Destructor:
 * -> Gracefully stops the listener and cleans up networking resources.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
class P2PClient
{
public:
    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * P2PClient
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Constructor initializes connection settings to the indexing server.
     *
     * Takes:
     * -> server_ip:
     *    IP address of the server.
     * -> server_port:
     *    Port number of the server.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    P2PClient(const std::string& server_ip, int server_port, const uint64_t uuid);

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * ~P2PClient
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Destructor attempts to shut down all network listeners and threads.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    ~P2PClient();

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * run
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Enters a command loop, accepting user commands like "index <file>",
     *    "download <file>", or "remove <file>". Internally, these commands
     *    use create*() and parse*() functions from messageFormatting.hpp to
     *    talk with the index server or other peers.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    void run();

    void handleSignal();

private:
    std::condition_variable chunks_ready;
    std::queue<size_t> remaining_chunks;
    std::queue<size_t> done_chunks;
    std::atomic<bool> download_complete{false};

    // -- High-level Command Handlers (e.g., user typed "index <filename>")
    void handleIndex(const std::string& file_name);
    void handleDownload(const uint64_t file_uuid);
    void handleDrop(const std::string& file_name);
    void printHelp();

    // -- Index Server Communication --
    /*
     * registerFile
     * Description: Creates and sends an INDEX_REQUEST message to the server,
     *              then waits for INDEX_OK or FAIL.
     *
     * Takes:
     * -> file_id:
     *    A 64-bit file ID for the file being indexed.
     * -> file_size:
     *    The file's total size (for informational purposes).
     *
     * Returns:
     * -> true if the server responds with INDEX_OK
     * -> false on error or FAIL message
     */
    bool registerFile(uint64_t file_id, uint64_t file_size);

    /*
     * removeFile
     * Description: Creates and sends a DROP_REQUEST message to the server,
     *              then waits for DROP_OK or FAIL.
     *
     * Takes:
     * -> file_id:
     *    The 64-bit file ID.
     * -> client_id:
     *    The 64-bit ID of this client (if your system tracks the client by ID).
     *
     * Returns:
     * -> true if the server responds with DROP_OK
     * -> false on error or FAIL message
     */
    bool removeFile(uint64_t file_id, uint64_t client_id);

    /*
     * findFilePeers
     * Description: Sends a SOURCE_REQUEST for the given file ID, then parses
     *              the server's SOURCE_LIST (or a FAIL) in response.
     *
     * Takes:
     * -> file_id:
     *    The 64-bit file ID to query.
     *
     * Returns:
     * -> A vector of SourceInfo for each peer that has indexed this file.
     * -> Empty on error or if no peers found (the server might also send FAIL).
     */
    std::vector<SourceInfo> findFilePeers(uint64_t file_id);

    /*
     * removeAllFiles
     * Description: Removes all currently shared files from the server.
     *              Called during shutdown to ensure clean client exit.
     */
    void removeAllFiles();

    void setRunning(bool running);

    // -- Listening for P2P requests --
    /*
     * startListening
     * Description: Opens a server socket (via socket.hpp) and spawns
     *              my_listen_thread to accept incoming peer connections.
     */
    void startListening();

    /*
     * listeningLoop
     * Description: The main loop that runs in my_listen_thread. It blocks on
     *              accept(), then hands each peer socket to handlePeerRequest().
     */
    void listeningLoop();

    /*
     * handlePeerRequest
     * Description: Handles a single peer's incoming request.
     *              This may include responding to DOWNLOAD_INIT,
     *              sending file chunks, or handling other requests
     *              defined in messageFormatting.hpp.
     */
    void handlePeerRequest(int client_sock);

    /*
     * stopAllSharing
     * Description: Shuts down my_listen_sock and joins my_listen_thread.
     */
    void stopAllSharing();

    // -- Peer-to-Peer Download --
    /*
     * downloadFromPeer
     * Description: Connects to the peer via socket.hpp, then sends a
     *              DOWNLOAD_INIT message, followed by repeated REQUEST_CHUNK
     *              messages. Receives DATA_CHUNK messages until the entire
     *              file is downloaded. Concludes with FINISH_DOWNLOAD.
     *
     * Takes:
     * -> peer_ip:
     *    The IPv4/IPv6 address of the peer.
     * -> peer_port:
     *    The port number of the peer.
     * -> file_id:
     *    The 64-bit file ID for the file to download.
     * -> local_name:
     *    The local filename where the downloaded data should be saved.
     *
     * Returns:
     * -> true on success
     * -> false on failure (e.g., if the peer sends FAIL or times out)
     */
    bool downloadFromPeer(const std::string&  peer_ip,
                          int                 peer_port,
                          uint64_t           file_id,
                          const std::string& local_name);

    /*
     * sendFileChunk
     * Description: Reads a portion of the local file from offset to offset+length,
     *              wraps it in a DATA_CHUNK message, and sends it back to the peer.
     *
     * Takes:
     * -> sock:
     *    The socket FD connected to the requesting peer.
     * -> file_id:
     *    The file's 64-bit ID (mainly for logging, if needed).
     * -> offset:
     *    The byte offset from which to read.
     * -> length:
     *    The number of bytes to read and send.
     */
    void sendFileChunk(int      sock,
                       uint64_t file_id,
                       long     offset,
                       long     length);

    // -- Utility --
    /*
     * getLocalIPAddress
     * Description: Returns the local IP address (e.g. "192.168.x.x").
     */
    std::string getLocalIPAddress();

    /*
     * getListeningPort
     * Description: Returns the ephemeral port on which we are listening
     *              for incoming P2P connections.
     */
    int getListeningPort();

    void workerThread(const uint64_t file_uuid, const std::vector<dfd::SourceInfo>& peers, size_t thread_ind);
    void downloadChunk(int client_socket_fd, const std::string& f_name, uint64_t f_size, size_t chunk_index);

private:
    uint64_t    my_uuid;
    SourceInfo  server_info;
    bool        am_running;

    // For each file we share: file_id -> local filename
    // If you still have string-based "UUIDs" in your code, you can keep them
    // here and convert to uint64_t in your .cpp implementations.
    std::map<uint64_t, std::string> shared_files_;
    std::mutex                      share_mutex_;
    std::mutex                      remaining_chunks_mutex;
    std::mutex                      done_chunks_mutex;

    int              my_listen_sock;
    uint16_t         my_listen_port;
    std::thread      my_listen_thread;
};

} // namespace dfd
