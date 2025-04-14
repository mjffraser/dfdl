#pragma once

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>
#include <optional>

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
 * -> Unpacks the above message, and returns a std::string of the received error
 *    message.
 * 
 * Takes:
 * -> fail_message:
 *    A message received who's std::vector::front references the FAIL err code.
 *
 * Returns:
 * -> On success:
 *    The error message.
 * -> On failure:
 *    An empty string. If the received message starts with a FAIL err code this
 *    should never happen.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::string parseFailMessage(const std::vector<uint8_t>& fail_message);

//SERVER MESSAGE CODES AND FUNCTIONS
inline constexpr uint8_t INDEX_REQUEST      = 0x01;
inline constexpr uint8_t INDEX_FORWARD      = 0x21;
inline constexpr uint8_t INDEX_OK           = 0x02;
inline constexpr uint8_t DROP_REQUEST       = 0x03;
inline constexpr uint8_t DROP_FORWARD       = 0x23;
inline constexpr uint8_t DROP_OK            = 0x04;
inline constexpr uint8_t REREGISTER_REQUEST = 0x05;
inline constexpr uint8_t REREGISTER_FORWARD = 0x25;
inline constexpr uint8_t REREGISTER_OK      = 0x06;
inline constexpr uint8_t SOURCE_REQUEST     = 0x07;
inline constexpr uint8_t SOURCE_LIST        = 0x08;
inline constexpr uint8_t FORWARD_OK         = 0x2F;
inline constexpr uint8_t CONTROL_REQUEST    = 0xA1;
inline constexpr uint8_t CONTROL_OK         = 0xA2;

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
std::vector<uint8_t> createIndexRequest(const FileId& file_info);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseIndexRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a FileId struct of the received
 *    data.
 * 
 * Takes:
 * -> index_message:
 *    A message received who's std::vector::front references the INDEX_REQUEST
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
std::vector<uint8_t> createDropRequest(const IndexUuidPair& uuids);

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
 *    A message received who's std::vector::front references the DROP_REQUEST 
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
 * -> Unpacks the above message, and returns a SourceInfo struct of the received
 *    data. Does not include uuid.
 * 
 * Takes:
 * -> reregister_message:
 *    A message received who's std::vector::front references the
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
 * -> Unpacks the above message, and returns the received file uuid.
 * 
 * Takes:
 * -> request_message:
 *    A message received who's std::vector::front references the SOURCE_REQUEST 
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
 * -> Unpacks the above message, and returns the received list of SourceInfo
 *    objects.
 * 
 * Takes:
 * -> request_message:
 *    A message received who's std::vector::front references the SOURCE_LIST 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The list of SourceInfo's. 
 * -> On failure:
 *    An empty list. The server should send an error message instead to convey 
 *    no indexers found.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
std::vector<SourceInfo> parseSourceList(std::vector<uint8_t> list_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createReregisterRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for writing all needed info in an unambiguous manner that
 *    can be easily unpacked by the below function.
 *
 * Takes:
 * -> faulty_client:
 *    The SourceInfo object.
 * -> file_id:
 *    The file uuid. 
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createControlRequest(const SourceInfo& faulty_client, const uint64_t file_id);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseReregisterRequest 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns a SourceInfo struct of the received
 *    data.
 * 
 * Takes:
 * -> control_message:
 *    A message received who's std::vector::front references the
 *    CONTROL_REQUEST code.
 *
 * Returns:
 * -> On success:
 *    The pair of file uuid and SourceInfo struct. 
 * -> On failure:
 *    The pair of file uuid and SourceInfo struct. port will be set to 0 to indicate error.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::pair<uint64_t, SourceInfo> parseControlRequest(const std::vector<uint8_t>& control_message);

//CLIENT MESSAGE CODES AND FUNCTIONS
inline constexpr uint8_t DOWNLOAD_INIT      = 0x09;
inline constexpr uint8_t DOWNLOAD_CONFIRM   = 0x0A;
inline constexpr uint8_t REQUEST_CHUNK      = 0x0B;
inline constexpr uint8_t DATA_CHUNK         = 0x0C;
inline constexpr uint8_t FINISH_DOWNLOAD    = 0x0D; //simple ack, just send byte
inline constexpr uint8_t FINISH_OK          = 0x0E;


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
 * -> Unpacks the above message, and returns the received pair of file uuid and
 *    file chunk size. If size is std::nullopt the default is assumed.
 * Takes:
 * -> request_message:
 *    A message received who's std::vector::front references the DOWNLOAD_INIT 
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
 * createDownloadConfirm
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a buffer for communicating a file size and name to the client
 *    before it attempts downloading.
 * Takes:
 * -> f_size:
 *    The file size.
 * -> f_name:
 *    The file name.
 *
 * Returns:
 * -> On success:
 *    The buffer. 
 * -> On failure:
 *    An empty buffer. 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::vector<uint8_t> createDownloadConfirm(const uint64_t f_size, const std::string& f_name);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseDownloadConfirm 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and returns the received pair of file_size, and
 *    file name, in that order.
 *
 * Takes:
 * -> confirm_message:
 *    A message received who's std::vector::front references the DOWNLOAD_INIT 
 *    code.
 *
 * Returns:
 * -> On success:
 *    The pair. 
 * -> On failure:
 *    A pair with uuid set to 0. 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
std::pair<uint64_t, std::string> parseDownloadConfirm(const std::vector<uint8_t> confirm_message);


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
 *    A message received who's std::vector::front references the REQUEST_CHUNK 
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
 *    receiving chunk n-1, you should be receiving chunk_size bytes.
 *
 * Takes:
 * -> data_chunk_message:
 *    A message received who's std::vector::front references the DATA_CHUNK
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


//SERVER REGISTRATION MESSAGES
inline constexpr uint8_t SERVER_REG         = 0x0F;
inline constexpr uint8_t CLIENT_REG         = 0x10; //just send this byte to get the server list
inline constexpr uint8_t REG_SERVERS_LIST   = 0x11;
inline constexpr uint8_t FORWARD_SERVER_REG = 0x12;
inline constexpr uint8_t FORWARD_SERVER_OK  = 0x13; //ack with just this byte

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createNewServerReg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a server-server registration request. This tells another server
 *    you'd like to register yourself into the network. The other server will
 *    then reply with a list of every server you're registered with, excluding
 *    itself. That registration happens due to this message.
 *
 * Takes:
 * -> new_server:
 *    The SourceInfo object with the IP and port of the server sending this
 *    request.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createNewServerReg(const SourceInfo& new_server);

/* 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseNewServerReg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and extracts the SourceInfo object that was
 *    passed in to create the NewServerReg message.
 *
 * Takes:
 * -> new_server_message:
 *    A message received who's std::vector::front references the NEW_SERVER_REG
 *    code.
 *
 * Returns:
 * -> On success:
 *    A SourceInfo object with the embedded IP and port.
 * -> On failure:
 *    A SourceInfo object with the port set to 0.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
SourceInfo parseNewServerReg(const std::vector<uint8_t>& new_server_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createServerRegResponse
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Encapsulates a list of servers (represented as SourceInfo objects with the
 *    IP and port set) into a message to be sent over a network. It's expected
 *    that every SourceInfo object in the vector have a valid IP and port set.
 *
 * -> This message should be sent in reply to either a NEW_SERVER_REG message
 *    or a NEW_CLIENT_REG message. In the case of the server-server message
 *    that server should register the new address to every other server it
 *    knows and acks a FORWARD_SERVER_REG message with, and then return this
 *    list to that new server. In the case of the client-server message that
 *    server should just return this message formed instead with just every
 *    server it knows about.
 *
 * Takes:
 * -> servers:
 *    The list of servers to send.
 *
 * Returns:
 * -> On success:
 *    The buffer to send over the socket.
 * -> On failure:
 *    An empty buffer.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<uint8_t> createServerRegResponse(const std::vector<SourceInfo>& servers);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseServerRegResponse
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and extracts the std::vector<SourceInfo> that
 *    was passed in to create the ServerRegResponse message.
 *
 * Takes:
 * -> new_server_message:
 *    A message received who's std::vector::front references the
 *    REG_SERVERS_LIST code.
 *
 * Returns:
 * -> On success:
 *    The vector of servers.
 * -> On failure:
 *    An empty vector.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::vector<SourceInfo> parseServerRegResponse(const std::vector<uint8_t>& reg_response);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createForwardServerReg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Deescription:
 * -> Takes a NEW_SERVER_REG message, and modifies it's message code to
 *    FORWARD_SERVER_REG.
 *
 * -> In the event of receiving a message with a
 *    NEW_SERVER_REG code, that server should use this function to alter that
 *    message and forward it to every server it knows. That server will use
 *    the received acks to build its list for createServerRegResponse().
 *
 * Takes:
 * -> new_server_message:
 *    A message received who's std::vector::front references the NEW_SERVER_REG
 *    code.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int createForwardServerReg(std::vector<uint8_t>& new_server_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * parseForwardServerReg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Unpacks the above message, and extracts the SourceInfo object that was
 *    passed in to create the ServerRegResponse that was altered into a
 *    FORWARD_SERVER_REG message.
 *
 * Takes:
 * -> forward_reg_message:
 *    A message received who's std::vector::front references the
 *    FORWARD_SERVER_REG code.
 *
 * Returns:
 * -> On success:
 *    The SourceInfo object for the new server in the network.
 * -> On failure:
 *    A SourceInfo object with the port set to 0.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
SourceInfo parseForwardServerReg(const std::vector<uint8_t>& forward_reg_message);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createForwardIndes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a INDEX_REQUEST message, and modifies it's message code to
 *    INDEX_FORWARD.
 *
 * -> new request type used to ekko requests without gettting an ekko back.
 *
 * Takes:
 * -> new_index:
 *    A message received who's std::vector::front references the INDEX_REQUEST code.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int createForwardIndex(std::vector<uint8_t>& new_index);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createForwardDrop
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a DROP_REQUEST message, and modifies it's message code to
 *    DROP_FORWARD.
 *
 * -> new request type used to ekko requests without gettting an ekko back.
 *
 * Takes:
 * -> new_drop:
 *    A message received who's std::vector::front references the DROP_REQUEST code.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int createForwardDrop(std::vector<uint8_t>& new_drop);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createForwardRereg
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a REREGISTER_REQUEST message, and modifies it's message code to
 *    REREGISTER_FORWARD.
 *
 * -> new request type used to ekko requests without gettting an ekko back.
 *
 * Takes:
 * -> new_rereg:
 *    A message received who's std::vector::front references the REREGISTER_REQUEST code.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int createForwardRereg(std::vector<uint8_t>& new_rereg);

inline constexpr uint8_t ELECT_LEADER = 0x14;
inline constexpr uint8_t ELECT_X      = 0x15;
inline constexpr uint8_t LEADER_X     = 0x16;
inline constexpr uint8_t BULLY        = 0x17;

inline constexpr uint8_t KEEP_ALIVE = 0x18;

} //dfd
