#include "client/internal/internal/attemptPeerRequest.hpp"
#include "client/internal/internal/internal/clientNetworking.hpp"
#include "client/internal/internal/internal/downloadHandshake.hpp"
#include "networking/fileParsing.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/socket.hpp"

#include <algorithm>

namespace dfd {

int selectPeerSource(std::vector<bool>& f_stats) {
    auto it = std::find_if(f_stats.begin(),
                           f_stats.end(),
                           [](bool b){return b;});
    if (it == f_stats.end())
        return -1;

    int ind = std::distance(f_stats.begin(), it);
    f_stats[ind] = false;
    return ind;
}

int attemptInitialChunkDownload(const  uint64_t                        f_uuid,       
                                       std::string&                    f_name,
                                       uint64_t&                       f_size,
                                       std::unique_ptr<std::ofstream>& file_ptr,
                                const  SourceInfo&                     server,
                                struct timeval                         connection_timeout,
                                struct timeval                         response_timeout) {
    int sock = connectToSource(server, connection_timeout); 
    if (sock < 0) return EXIT_FAILURE;

    if (EXIT_FAILURE == attemptDownloadHandshake(sock,
                                                 f_uuid,
                                                 f_name,
                                                 f_size,
                                                 response_timeout)) {
        return EXIT_FAILURE; //socket already closed
    }

    std::vector<uint8_t> chunk_request = createChunkRequest(0);
    std::vector<uint8_t> data_chunk_msg;
    int res = sendAndRecv(sock,
                          chunk_request,
                          data_chunk_msg,
                          DATA_CHUNK,
                          response_timeout);
    if (res == EXIT_FAILURE) {
        closeSocket(sock);
        return EXIT_FAILURE;
    }

    //send finish message and close connection
    sendOkay(sock, {FINISH_DOWNLOAD});
    closeSocket(sock);

    //peer communication finished, now start file
    DataChunk dc = parseDataChunk(data_chunk_msg);
    if (dc.first == SIZE_MAX)
        return EXIT_FAILURE;

    if (EXIT_FAILURE == unpackFileChunk(f_name,
                                        dc.second,
                                        dc.second.size(),
                                        dc.first)) {
        return EXIT_FAILURE;
    }

    file_ptr = openFile(f_name);
    if (file_ptr == nullptr)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

}
