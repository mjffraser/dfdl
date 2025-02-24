#pragma once

#include "server/internal/database/internal/types.hpp"
#include <utility>

namespace dfd {

// TABLE NAMES
inline constexpr char PEER_NAME [] = "PEERS";
inline constexpr char FILE_NAME [] = "FILES";
inline constexpr char INDEX_NAME[] = "FILE_INDEX";

// PRIMARY KEYS
inline const TableKey PEER_KEY  = {"id",      "INT"};
inline const TableKey FILE_KEY  = {"id",      "INT"};
inline const TableKey INDEX_KEY = {"pid_fid", "TEXT"}; //derived manually for ease of use

// ATTRIBUTES DEFINITIONS
inline const std::vector<TableKey> PEER_ATTRIBUTES = {
    std::make_pair("ip_addr", "TEXT"),
    std::make_pair("port",    "INT") 
};

inline const std::vector<TableKey> FILE_ATTRIBUTES = {
    std::make_pair("size", "INT"),
};

inline const std::vector<TableKey> INDEX_ATTRIBUTES = {
    std::make_pair("client_id", "INT"),
    std::make_pair("index_id",  "INT")
};

// FOREIGN KEYS
inline const std::vector<ForeignKey> INDEX_FK = {
    std::make_tuple(INDEX_ATTRIBUTES[0].first, PEER_NAME, PEER_KEY.first), //client_id
    std::make_tuple(INDEX_ATTRIBUTES[1].first,  FILE_NAME, FILE_KEY.first) //index_id
};

} //dfd
