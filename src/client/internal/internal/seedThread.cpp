#include "client/internal/internal/seedThread.hpp"
#include "client/internal/internal/internal/clientNetworking.hpp"
#include "networking/fileParsing.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"
#include <iostream>
#include <map>
#include <thread>
#include <unistd.h>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * errScenario
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a FAIL message to send to the connected peer. Sends the message,
 *    then closes the socket. Returns EXIT_FAILURE to report as the exit code
 *    for the calling function.
 *
 * Takes:
 * -> msg:
 *    The error message to encapsulate in the FAIL message.
 * -> sock:
 *    The connected peer socket to close.
 *
 * Returns:
 * -> On success:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int errScenario(const std::string& msg, int sock) {
    std::vector<uint8_t> fail_msg = createFailMessage(msg);
    sendOkay(sock, fail_msg);
    closeSocket(sock);
    return EXIT_FAILURE;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * initHandshake
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Receives a DOWNLOAD_INIT message from the client, parses it, checks that
 *    we know where the file is, reads in the file size, and replies with a
 *    DOWNLOAD_CONFIRM message. If a failure occurs at any point in this
 *    handshake, the socket is closed, and an error is returned. If appropriate,
 *    a FAIL message is sent to the client with the reason for the error so they
 *    can deregister us as a peer hosting this file.
 *
 * Takes:
 * -> peer_sock:
 *    The socket connected to the requesting peer.
 * -> indexed_files:
 *    A map of all files this client currently has indexed.
 * -> indexed_files_mtx:
 *    A mutex to lock when accessing the indexed_files map.
 * -> timeout:
 *    A timeout for all received messages.
 * -> f_path:
 *    The path to the indexed file on the disk.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int initHandshake(int                                     peer_sock,
                  const  std::map<uint64_t, std::string>& indexed_files,
                  std::mutex&                             indexed_files_mtx,
                  struct timeval                          timeout,
                  std::filesystem::path&                  f_path) {
    std::cout << "starting" << std::endl;
    // recieve client download init request
    std::vector<uint8_t> client_init_msg;
    if (!recvOkay(peer_sock, client_init_msg, DOWNLOAD_INIT, timeout)) {
        closeSocket(peer_sock);
        return EXIT_FAILURE;
    }

    std::cout << "okay" << std::endl;

    //check for valid request
    auto [uuid, c_size] = parseDownloadInit(client_init_msg);
    {
        //lock indexed files for the read
        std::unique_lock<std::mutex> lock(indexed_files_mtx);
        auto it = indexed_files.find(uuid);
        if (uuid == 0 || it == indexed_files.end()) {
            closeSocket(peer_sock);
            return EXIT_FAILURE;
        }

        f_path = indexed_files.at(uuid);
    }
    
    //find file
    if (f_path.empty())
        return errScenario("[err] Could not find file. Sorry.", peer_sock);

    //find file size & chunks
    if (c_size.has_value()) setChunkSize(c_size.value());

    auto f_size_opt = fileSize(f_path);
    if (!f_size_opt.has_value())
        return errScenario("[err] Could not determine file size. Sorry.", peer_sock);

    //reply with confirmation to peer
    std::vector<uint8_t> confirm_msg = createDownloadConfirm(f_size_opt.value(),
                                                             f_path.filename());

    std::cout << "GREAT" << std::endl;
    if (sendOkay(peer_sock, confirm_msg))
        return EXIT_SUCCESS;
    std::cout << "????" << std::endl;
    closeSocket(peer_sock);
    return EXIT_FAILURE;
}

void seedToPeer(std::atomic<bool>&                     shutdown,
                int                                    peer_sock,
                const std::map<uint64_t, std::string>& indexed_files,
                std::mutex&                            indexed_files_mtx) {
    struct timeval seed_timeout;
    seed_timeout.tv_sec  = 3;
    seed_timeout.tv_usec = 0;

    //handshake with peer, send peer needed info
    std::filesystem::path f_path; //set by handshake
    if (EXIT_FAILURE == initHandshake(peer_sock,
                                      indexed_files, 
                                      indexed_files_mtx,
                                      seed_timeout,
                                      f_path)) {
        return;
    }

    //wait for peer chunk requests
        std::vector<uint8_t> client_ask;
    while (recvOkay(peer_sock, client_ask, REQUEST_CHUNK, seed_timeout)) {
        size_t chunk_id = parseChunkRequest(client_ask); 

        //read chunk
        std::vector<uint8_t> chunk;
        auto res = packageFileChunk(f_path, chunk, chunk_id);
        if (!res) {
            //could not read file for some reason
            std::vector<uint8_t> fail_msg = createFailMessage("Sorry, file appears to be unavailable.");
            sendOkay(peer_sock, fail_msg);
            break;
        }

        //send chunk
        chunk.resize(res.value());
        DataChunk dc = {chunk_id, chunk};
        std::vector<uint8_t> chunk_msg = createDataChunk(dc);
        double X=((double)rand()/(double)RAND_MAX);
        std::this_thread::sleep_for(std::chrono::duration<double>(X)); //ARTIFICIAL DELAYS
        if (!sendOkay(peer_sock, chunk_msg)) break;
    }

    closeSocket(peer_sock); // Clean up the socket when done
}

} //dfd
