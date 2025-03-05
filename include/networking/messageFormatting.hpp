#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>

namespace dfd {

/* 
 * The first byte of every message is the type. 
 *
 * Based on the message type, feed it into the corresponding
 * function below to extract its data.
 *
 * All message codes ending in "OK" are simple acks. The singular
 * byte should be sent to the server/client to report status on
 * operations that don't return data, like a drop request.
 *
 * If the first byte is FAIL, an error occured. Client & server
 * should decide what to do from there, retry, return to last
 * stable state, etc.
 */

//GENERAL MESSAGE CODES AND FUNCTIONS
inline constexpr uint8_t FAIL               = 0x00;

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createFailMessage
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer with the error message, prepended with the FAIL err code.
 * 
 * Takes:
 * -> error_message:
 *    A string explaining what went wrong for the other side of the
 *    communication. Alternatively, for internal messages this can be used to 
 *    communicate state to recover from errors.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over a socket.
 * -> On failure:
 *    An empty buffer. Note that the FAIL code means this this is never confused
 *    with an empty message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::vector<uint8_t> createFailMessage(const std::string& error_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseFailMessage 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a std::string of the recieved error
 *    message.
 * 
 * Takes:
 * -> fail_message:
 *    A message recieved who's std::vector::front references the FAIL err code.
 *
 * Returns:
 * -> On success:
 *    The error message.
 * -> On failure:
 *    An empty string. If the recieved message starts with a FAIL err code this
 *    should never happen.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::string parseFailMessage(const std::vector<uint8_t>& fail_message);

//SERVER MESSAGE CODES AND FUNCTIONS
inline constexpr uint8_t INDEX_REQUEST      = 0x01;
inline constexpr uint8_t INDEX_OK           = 0xF1;
inline constexpr uint8_t DROP_REQUEST       = 0x02;
inline constexpr uint8_t DROP_OK            = 0xF2;
inline constexpr uint8_t REREGISTER_REQUEST = 0x03;
inline constexpr uint8_t REREGISTER_OK      = 0xF3;
inline constexpr uint8_t SOURCE_REQUEST     = 0x04;
inline constexpr uint8_t SOURCE_LIST        = 0xF4;

//small wrapper struct for passing all info needed to id
//file and indexer
struct FileId {
    uint64_t   uuid;
    SourceInfo indexer;
    uint64_t   f_size;

    FileId(uint64_t   id,
           SourceInfo s_inf,
           uint64_t   size)
           :
           uuid    (id),
           indexer (s_inf),
           f_size  (size) {}
};

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createIndexRequest
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function.
 *
 * Takes:
 * -> file_info:
 *    A FileId struct with all the required data to index a file. 
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createIndexRequest(FileId& file_info);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseIndexRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a FileId struct of the recieved
 *    data.
 * 
 * Takes:
 * -> index_message:
 *    A message recieved who's std::vector::front references the INDEX_REQUEST
 *    code.
 *
 * Returns:
 * -> On success:
 *    The FileId struct. 
 * -> On failure:
 *    A FileId struct. file uuid will be set to 0 to indicate error.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
FileId parseIndexRequest(const std::vector<uint8_t>& index_message);

//file uuid, then client uuid
using IndexUuidPair = std::pair<uint64_t, uint64_t>;
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createDropRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function.
 *
 * Takes:
 * -> file_info:
 *    A pair of file uuid, then client uuid. 
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createDropRequest(IndexUuidPair& uuids);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseDropRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a IndexUuidPair with the info
 *    needed to drop an index.
 * 
 * Takes:
 * -> drop_message:
 *    A message recieved who's std::vector::front references the DROP_REQUEST 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The pair of uuids. 
 * -> On failure:
 *    A pair of 0's.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
IndexUuidPair parseDropRequest(const std::vector<uint8_t>& drop_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createReregisterRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function. Note that UUID's cannot be
 *    updated in the database. A client should maintain a single UUID, updating
 *    IP & port when starting a new session as an indexer.
 *
 * Takes:
 * -> indexer:
 *    The SourceInfo object.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createReregisterRequest(const SourceInfo& indexer);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseReregisterRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a SourceInfo struct of the recieved
 *    data. Does not include uuid.
 * 
 * Takes:
 * -> reregister_message:
 *    A message recieved who's std::vector::front references the
 *    REREGISTER_REQUEST code.
 *
 * Returns:
 * -> On success:
 *    The SourceInfo struct. 
 * -> On failure:
 *    A SourceInfo struct. port will be set to 0 to indicate error.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
SourceInfo parseReregisterRequest(const std::vector<uint8_t>& reregister_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createSourceRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function. 
 *
 * Takes:
 * -> uuid:
 *    The file uuid to retrieve sources for. 
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createSourceRequest(const uint64_t uuid);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseSourceRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the recieved file uuid.
 * 
 * Takes:
 * -> request_message:
 *    A message recieved who's std::vector::front references the SOURCE_REQUEST 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The file uuid.
 * -> On failure:
 *    0. This might be a possible hash, but nobody found one to date.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
uint64_t parseSourceRequest(const std::vector<uint8_t>& request_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createSourceList 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function. 
 *
 * Takes:
 * -> source_list:
 *    A vector of all SourceInfo sources that are indexing the file. These are
 *    serialized in a paticular manner so the below function can unpack them.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createSourceList(const std::vector<SourceInfo>& source_list);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseSourceList 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the recieved list of SourceInfo
 *    objects.
 * 
 * Takes:
 * -> request_message:
 *    A message recieved who's std::vector::front references the SOURCE_LIST 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The list of SourceInfo's. 
 * -> On failure:
 *    An empty list. The server should send an error message instead to convey 
 *    no indexers found.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::vector<SourceInfo> parseSourceList(std::vector<uint8_t> list_message);

//CLIENT MESSAGE CODES AND FUNCTIONS
inline constexpr uint8_t DOWNLOAD_INIT      = 0x05;
inline constexpr uint8_t DOWNLOAD_OK        = 0xF5;
inline constexpr uint8_t REQUEST_CHUNK      = 0x06;
inline constexpr uint8_t DATA_CHUNK         = 0xF6;
inline constexpr uint8_t FINISH_DOWNLOAD    = 0x07; //simple ack, just send byte
inline constexpr uint8_t FINISH_OK          = 0xF7;


/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createDownloadInit 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function. Chunk size is only needed if
 *    it was modified in fileParsing. Otherwise, the default is assumed.
 *
 * Takes:
 * -> uuid:
 *    The uuid of the file to download.
 * -> chunk_size:
 *    Possibly the chunk size to send with. If std::nullopt the default is
 *    assumed.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createDownloadInit(const uint64_t uuid, std::optional<size_t> chunk_size);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseDownloadInit 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the recieved pair of file uuid and
 *    file chunk size. If size is std::nullopt the default is assumed.
 * Takes:
 * -> request_message:
 *    A message recieved who's std::vector::front references the DOWNLOAD_INIT 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The pair. 
 * -> On failure:
 *    A pair with uuid set to 0. 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::pair<uint64_t, std::optional<size_t>> parseDownloadInit(const std::vector<uint8_t> init_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createChunkRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function.
 *
 * Takes:
 * -> chunk:
 *    The chunk to request, 0-indexed. 
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createChunkRequest(const size_t chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseChunkRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the chunk index requested. 
 *
 * Takes:
 * -> request_message:
 *    A message recieved who's std::vector::front references the REQUEST_CHUNK 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The chunk index. 
 * -> On failure:
 *    SIZE_MAX. If you're requesting SIZE_MAX chunks, that's on you.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
size_t parseChunkRequest(const std::vector<uint8_t>& request_message);

//pair of actual data, and the chunk number, 0-indexed
using DataChunk = std::pair<size_t, std::vector<uint8_t>>;
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createDataChunk 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function.
 *
 * Takes:
 * -> chunk:
 *    The DataChunk to send.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createDataChunk(const DataChunk& chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseDataChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the DataChunk. Note that a data
 *    chunk could be any size. It's up to the caller to verify the size of the
 *    actual data returned in the DataChunk. For every chunk [1..n), unless
 *    recieving chunk n-1, you should be recieving chunk_size bytes.
 *
 * Takes:
 * -> data_chunk_message:
 *    A message recieved who's std::vector::front references the DATA_CHUNK
 *    code.
 *
 *
 * Returns:
 * -> On success:
 *    The pair of chunk index, and the byte data.
 * -> On failure:
 *    <SIZE_MAX, {}>
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
DataChunk parseDataChunk(const std::vector<uint8_t>& data_chunk_message);

} //dfd
