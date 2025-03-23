#include "networking/messageFormatting.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include <cstdlib>
#include <cstring>
#include <endian.h>
#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <sys/socket.h>

namespace dfd {

std::vector<uint8_t> createFailMessage(const std::string& error_message) {
    if (error_message.length() <= 0)
        return {};

    std::vector<uint8_t> message_buff = {FAIL};
    message_buff.resize(1+error_message.length());
    std::memcpy(message_buff.data()+1, error_message.c_str(), error_message.length());

    return message_buff;
}

std::string parseFailMessage(const std::vector<uint8_t>& fail_message) {
    if (fail_message.size() < 2) {
        return "";
    } else if (*fail_message.begin() != FAIL) {
        return "";
    }
    return std::string(fail_message.begin()+1, fail_message.end());
}

//SERVER MESSAGE CODES AND FUNCTIONS

//ONLY DEFINED IF dest IS SIZED TO ALWAYS HAVE ROOM
//puts inputs in network-byte order (BE)
//if a std::string is given as input, it's assumed to be a IPv4 addr, and inet
//functions are used to cast IP into uint32_t.
template <typename T>
void createNetworkData(uint8_t* dest, const T data, size_t& offset, int& err_code) {
    if constexpr (std::is_same_v<T, std::string>) {
        uint32_t network_data = getIpBytes(data); //inet_pton() was called to convert IP, so already in BE
        if (network_data == 0)
            err_code = 1;
        std::memcpy(dest+offset, &network_data, sizeof(uint32_t));
        offset += sizeof(uint32_t);
    }  else {
        T network_data = toNetworkOrder(data, err_code);
        std::memcpy(dest+offset, &network_data, sizeof(T));
        offset += sizeof(T);
    }
}

std::vector<uint8_t> createIndexRequest(FileId& file_info) {
    uint32_t ip_bytes = getIpBytes(file_info.indexer.ip_addr);
    if (ip_bytes == 0) 
        return {};

    std::vector<uint8_t> index_buff = {INDEX_REQUEST};
    index_buff.resize(1+30); //one extra for code byte
    size_t offset = 1; //start on second byte in vector
    int err_code  = 0;

    //ORDER:
    //file uuid, file size, client port, client uuid, client ip
    createNetworkData(index_buff.data(), file_info.uuid,            offset, err_code);
    createNetworkData(index_buff.data(), file_info.f_size,          offset, err_code);
    createNetworkData(index_buff.data(), file_info.indexer.port,    offset, err_code);
    createNetworkData(index_buff.data(), file_info.indexer.peer_id, offset, err_code);
    createNetworkData(index_buff.data(), file_info.indexer.ip_addr, offset, err_code);


    return index_buff;
}

//endian-safe casting via fromNetworkOrder template function in byteOrdering.hpp
//if a std::string is passed as destination it's assumed to be an IPv4 addr,
//and inet parsing functions are used instead.
template <typename T>
void parseNetworkData(T* dest, const uint8_t* buff, size_t& offset, int& err_code) {
    if constexpr (std::is_same_v<T, std::string>) {
        std::string ip_str = ipBytesToString(buff+offset);
        if (ip_str == "")
            err_code = 1;
        *dest = ip_str;
        offset += sizeof(uint32_t); //stored as 4 byte in buffer
    } else {
        T network_data;
        std::memcpy(&network_data, buff+offset, sizeof(T));
        *dest = fromNetworkOrder(network_data, err_code);
        offset += sizeof(T);
    }
}

FileId parseIndexRequest(const std::vector<uint8_t>& index_message) {
    FileId f_id(0, SourceInfo(), 0);
    if (index_message.size() != 31) {
        return f_id;
    } else if (*index_message.begin() != INDEX_REQUEST && *index_message.begin() != INDEX_FORWARD) {
        return f_id;
    }

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createIndexRequest
    parseNetworkData(&f_id.uuid,            index_message.data(), offset, err_code);
    parseNetworkData(&f_id.f_size,          index_message.data(), offset, err_code);
    parseNetworkData(&f_id.indexer.port,    index_message.data(), offset, err_code);
    parseNetworkData(&f_id.indexer.peer_id, index_message.data(), offset, err_code);
    parseNetworkData(&f_id.indexer.ip_addr, index_message.data(), offset, err_code);

    if (err_code != 0)
        f_id.uuid = 0;

    return f_id;
}

std::vector<uint8_t> createDropRequest(IndexUuidPair& uuids) {
    if (uuids.first == 0 || uuids.second == 0)
        return {};

    std::vector<uint8_t> drop_buff = {DROP_REQUEST};
    drop_buff.resize(1+16); //8 bytes for each uuid

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //file uuid, client uuid
    createNetworkData(drop_buff.data(), uuids.first,  offset, err_code);
    createNetworkData(drop_buff.data(), uuids.second, offset, err_code);

    if (err_code != 0)
        return {};

    return drop_buff;
}

IndexUuidPair parseDropRequest(const std::vector<uint8_t>& drop_message) {
    IndexUuidPair pair(0,0);
    if (drop_message.size() != 17)
        return pair;
    else if (*drop_message.begin() != DROP_REQUEST && *drop_message.begin() != DROP_FORWARD)

        return pair;

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createDropRequest
    parseNetworkData(&pair.first, drop_message.data(), offset, err_code);
    parseNetworkData(&pair.second, drop_message.data(), offset, err_code);

    if (err_code != 0)
        return {0,0};

    return pair;
}

std::vector<uint8_t> createReregisterRequest(const SourceInfo& indexer) {
    std::vector<uint8_t> reregister_buff = {REREGISTER_REQUEST};
    reregister_buff.resize(1+14); //SourceInfo is 14 bytes

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //client port, client uuid, client ip_addr
    createNetworkData(reregister_buff.data(), indexer.port, offset, err_code);
    createNetworkData(reregister_buff.data(), indexer.peer_id, offset, err_code);
    createNetworkData(reregister_buff.data(), indexer.ip_addr, offset, err_code);

    if (err_code != 0)
        return {};

    return reregister_buff;
}

SourceInfo parseReregisterRequest(const std::vector<uint8_t>& reregister_message) {
    SourceInfo si; si.port = 0;
    if (reregister_message.size() != 15)
        return si;
    else if (*reregister_message.begin() != REREGISTER_REQUEST && *reregister_message.begin() != REREGISTER_FORWARD)
        return si;

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createReregisterRequest
    parseNetworkData(&si.port,    reregister_message.data(), offset, err_code);
    parseNetworkData(&si.peer_id, reregister_message.data(), offset, err_code);
    parseNetworkData(&si.ip_addr, reregister_message.data(), offset, err_code);

    if (err_code != 0)
        si.port = 0;

    return si;
}

std::vector<uint8_t> createSourceRequest(const uint64_t uuid) {
    std::vector<uint8_t> source_buffer = {SOURCE_REQUEST};
    source_buffer.resize(1+8);

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //file uuid
    createNetworkData(source_buffer.data(), uuid, offset, err_code);

    if (err_code != 0)
        return {};

    return source_buffer;
}

uint64_t parseSourceRequest(const std::vector<uint8_t>& request_message) {
    if (request_message.size() != 9)
        return 0;
    else if (*request_message.begin() != SOURCE_REQUEST)
        return 0;

    size_t offset = 1;
    int err_code  = 0;
    uint64_t uuid;

    //pull stuff out in the same order as it was inserted by createSourceRequest
    parseNetworkData(&uuid, request_message.data(), offset, err_code);

    if (err_code != 0)
        return 0;

    return uuid;
}

std::vector<uint8_t> createSourceList(const std::vector<SourceInfo>& source_list) {
    std::vector<uint8_t> list_buffer = {SOURCE_LIST};
    list_buffer.resize(1+(14*source_list.size())); //port: 2bytes, uuid: 8, ip: 4

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //port then client uuid then ip_addr, for every source
    for (auto& s : source_list) {
        createNetworkData(list_buffer.data(), s.port,    offset, err_code);
        createNetworkData(list_buffer.data(), s.peer_id, offset, err_code);
        createNetworkData(list_buffer.data(), s.ip_addr, offset, err_code);
    }

    if (err_code != 0)
        return {};

    return list_buffer;
}

std::vector<SourceInfo> parseSourceList(std::vector<uint8_t> list_message) {
    if (((list_message.size()-1) % 14) != 0)
        return {};
    else if (*list_message.begin() != SOURCE_LIST)
        return {};

    std::vector<SourceInfo> sources;
    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createSourceList
    for (size_t i = 0; i < ((list_message.size()-1)/14); ++i) {
        SourceInfo s;
        parseNetworkData(&s.port,    list_message.data(), offset, err_code);
        parseNetworkData(&s.peer_id, list_message.data(), offset, err_code);
        parseNetworkData(&s.ip_addr, list_message.data(), offset, err_code);
        sources.push_back(s);
    }

    if (err_code != 0)
        return {};

    return sources;
}

std::vector<uint8_t> createControlRequest(const SourceInfo& faulty_client, const uint64_t file_id) {
    std::vector<uint8_t> reregister_buff = {CONTROL_REQUEST};
    reregister_buff.resize(1+22); //SourceInfo is 14 bytes, file_id is 8 bytes

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //client port, client uuid, client ip_addr
    createNetworkData(reregister_buff.data(), faulty_client.port, offset, err_code);
    createNetworkData(reregister_buff.data(), faulty_client.peer_id, offset, err_code);
    createNetworkData(reregister_buff.data(), faulty_client.ip_addr, offset, err_code);
    createNetworkData(reregister_buff.data(), file_id, offset, err_code);

    if (err_code != 0)
        return {};

    return reregister_buff;
}

std::pair<uint64_t, SourceInfo> parseControlRequest(const std::vector<uint8_t>& control_message) {
    SourceInfo si; si.port = 0;
    uint64_t file_id;
    if (control_message.size() != 23)
        return {};
    else if (*control_message.begin() != CONTROL_REQUEST)
        return {};

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createReregisterRequest
    parseNetworkData(&si.port,    control_message.data(), offset, err_code);
    parseNetworkData(&si.peer_id, control_message.data(), offset, err_code);
    parseNetworkData(&si.ip_addr, control_message.data(), offset, err_code);
    parseNetworkData(&file_id,    control_message.data(), offset, err_code);

    if (err_code != 0)
        si.port = 0;
        file_id = 0;

    std::pair<uint64_t, SourceInfo> pair(file_id, si);
    return pair;
}

//CLIENT MESSAGE CODES AND FUNCTIONS

std::vector<uint8_t> createDownloadInit(const uint64_t uuid, std::optional<size_t> chunk_size) {
    std::vector<uint8_t> init_buffer = {DOWNLOAD_INIT};
    init_buffer.resize(1+16);
    uint64_t c_size = 0;
    if (chunk_size)
        c_size = chunk_size.value();

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //file uuid, chunk size
    createNetworkData(init_buffer.data(), uuid,   offset, err_code);
    createNetworkData(init_buffer.data(), c_size, offset, err_code);

    if (err_code != 0)
        return {};

    return init_buffer;
}

std::pair<uint64_t, std::optional<size_t>> parseDownloadInit(const std::vector<uint8_t> init_message) {
    if (init_message.size() != 17)
        return {};
    else if (*init_message.begin() != DOWNLOAD_INIT)
        return {};

    std::pair<uint64_t, std::optional<size_t>> pair(0, std::nullopt);
    size_t offset = 1;
    int err_code    = 0;
    uint64_t c_size = 0;

    //pull stuff out in the same order as it was inserted by createDownloadInit
    parseNetworkData(&pair.first, init_message.data(), offset, err_code);
    parseNetworkData(&c_size,     init_message.data(), offset, err_code);

    if (err_code != 0)
        return {0, std::nullopt};

    if (c_size != 0)
        pair.second = c_size;

    return pair;
}

std::vector<uint8_t> createDownloadConfirm(const uint64_t f_size, const std::string& f_name) {
    std::vector<uint8_t> confirm_buffer = {DOWNLOAD_CONFIRM};
    confirm_buffer.resize(1+8+f_name.size());
    
    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //file size
    createNetworkData(confirm_buffer.data(), f_size, offset, err_code);
    std::memcpy(confirm_buffer.data()+offset, f_name.c_str(), f_name.size());

    if (err_code != 0)
        return {};

    return confirm_buffer;
}

std::pair<uint64_t, std::string> parseDownloadConfirm(const std::vector<uint8_t> confirm_message) {
    if (*confirm_message.begin() != DOWNLOAD_CONFIRM)
        return {};

    size_t offset = 1;
    int err_code  = 0;
    std::pair<uint64_t, std::string> file_info;

    parseNetworkData(&file_info.first, confirm_message.data(), offset, err_code);
    std::string f_name(confirm_message.begin()+offset, confirm_message.end());
    file_info.second = f_name;

    if (err_code != 0)
        return {0, ""};

    return file_info;
}

std::vector<uint8_t> createChunkRequest(const size_t chunk) {
    std::vector<uint8_t> chunk_buff = {REQUEST_CHUNK};
    chunk_buff.resize(1+8);
    uint64_t c = chunk;

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //chunk index
    createNetworkData(chunk_buff.data(), c, offset, err_code);

    if (err_code != 0)
        return {};

    return chunk_buff;
}

size_t parseChunkRequest(const std::vector<uint8_t>& request_message) {
    if (request_message.size() != 9)
        return SIZE_MAX;
    else if (*request_message.begin() != REQUEST_CHUNK)
        return SIZE_MAX;

    uint64_t chunk;
    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createChunkRequest
    parseNetworkData(&chunk, request_message.data(), offset, err_code);

    if (err_code != 0)
        return SIZE_MAX;

    return (size_t)chunk;
}

std::vector<uint8_t> createDataChunk(const DataChunk& chunk) {
    std::vector<uint8_t> data_buff = {DATA_CHUNK};
    data_buff.resize(1+8+chunk.second.size());
    uint64_t c = chunk.first;

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //chunk index, chunk data
    createNetworkData(data_buff.data(), c, offset, err_code);
    std::memcpy(data_buff.data()+offset, chunk.second.data(), chunk.second.size());

    if (err_code != 0)
        return {};

    return data_buff;
}

DataChunk parseDataChunk(const std::vector<uint8_t>& data_chunk_message) {
    //can't check len here
    if (*data_chunk_message.begin() != DATA_CHUNK)
        return {SIZE_MAX, {}};

    std::vector<uint8_t> data;
    data.resize(data_chunk_message.size()-9);
    uint64_t c;

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createDataChunk
    parseNetworkData(&c, data_chunk_message.data(), offset, err_code);
    if (err_code != 0)
        return {SIZE_MAX, {}};

    std::memcpy(data.data(), data_chunk_message.data()+offset, data_chunk_message.size()-9);

    return {(size_t)c, data};
}

std::vector<uint8_t> createNewServerReg(const SourceInfo& new_server) {
    std::vector<uint8_t> reg_buff = {SERVER_REG};
    reg_buff.resize(1+6);

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //port, IP
    createNetworkData(reg_buff.data(), new_server.port,    offset, err_code);
    createNetworkData(reg_buff.data(), new_server.ip_addr, offset, err_code);

    if (err_code != 0)
        return {};

    return reg_buff;
}

SourceInfo parseNewServerReg(const std::vector<uint8_t>& new_server_message) {
    SourceInfo si; si.port = 0;
    if (new_server_message.size() != 7)
        return si;
    if (*new_server_message.begin() != SERVER_REG)
        return si;

    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createNewServerReg
    parseNetworkData(&si.port,    new_server_message.data(), offset, err_code);
    parseNetworkData(&si.ip_addr, new_server_message.data(), offset, err_code);

    if (err_code != 0)
        si.port = 0;

    return si;
}

std::vector<uint8_t> createServerRegResponse(const std::vector<SourceInfo>& servers) {
    std::vector<uint8_t> response_buff = {REG_SERVERS_LIST};
    response_buff.resize(1+(6*servers.size()));

    size_t offset = 1;
    int err_code  = 0;

    //ORDER:
    //[port, IP] servers.size() times
    for (size_t i = 0; i < servers.size(); ++i) {
        createNetworkData(response_buff.data(), servers[i].port,    offset, err_code);
        createNetworkData(response_buff.data(), servers[i].ip_addr, offset, err_code);
    }

    if (err_code != 0)
        return {};

    return response_buff;
}

std::vector<SourceInfo> parseServerRegResponse(const std::vector<uint8_t>& reg_response) {
    if (((reg_response.size()-1) % 6) != 0)
        return {};
    if (*reg_response.begin() != REG_SERVERS_LIST)
        return {};

    std::vector<SourceInfo> sources;
    size_t offset = 1;
    int err_code  = 0;

    //pull stuff out in the same order as it was inserted by createServerRegResponse
    for (size_t i = 0; i < ((reg_response.size()-1)/6); ++i) {
        SourceInfo si;
        parseNetworkData(&si.port,    reg_response.data(), offset, err_code);
        parseNetworkData(&si.ip_addr, reg_response.data(), offset, err_code);
        sources.push_back(si); 
    }

    if (err_code != 0)
        return {};

    return sources;
}

int createForwardServerReg(std::vector<uint8_t>& new_server_message) {
    if (new_server_message.size() != 7)
        return EXIT_FAILURE;
    if (*new_server_message.begin() != SERVER_REG)
        return EXIT_FAILURE;
    new_server_message[0] = FORWARD_SERVER_REG;
    return EXIT_SUCCESS;
}

SourceInfo parseForwardServerReg(const std::vector<uint8_t>& forward_reg_message) {
    SourceInfo si; si.port = 0;
    if (forward_reg_message.size() != 7)
        return si;
    if (*forward_reg_message.begin() != FORWARD_SERVER_REG)
        return si;
    
    size_t offset = 1;
    int err_code  = 0;

    parseNetworkData(&si.port,    forward_reg_message.data(), offset, err_code);
    parseNetworkData(&si.ip_addr, forward_reg_message.data(), offset, err_code);

    if (err_code != 0)
        si.port = 0;

    return si;
}

int createForwardIndex(std::vector<uint8_t>& new_index) {
    if (new_index.size() < 1)
        return EXIT_FAILURE;
    if (*new_index.begin() != INDEX_REQUEST)
        return EXIT_FAILURE;
    new_index[0] = INDEX_FORWARD;
    return EXIT_SUCCESS;
}

int createForwardDrop(std::vector<uint8_t>& new_drop) {
    if (new_drop.size() < 1)
        return EXIT_FAILURE;
    if (*new_drop.begin() != DROP_REQUEST)
        return EXIT_FAILURE;
    new_drop[0] = DROP_FORWARD;
    return EXIT_SUCCESS;
}

int createForwardRereg(std::vector<uint8_t>& new_rereg) {
    if (new_rereg.size() < 1)
        return EXIT_FAILURE;
    if (*new_rereg.begin() != REREGISTER_REQUEST)
        return EXIT_FAILURE;
    new_rereg[0] = REREGISTER_FORWARD;
    return EXIT_SUCCESS;
}

} //dfd
