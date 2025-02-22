#pragma once

#include "server/internal/database/internal/types.hpp"
#include <utility>

namespace dfd {

// TABLE NAMES
inline constexpr char CLIENT_NAME      [] = "CLIENT";
inline constexpr char INDEX_NAME       [] = "INDEX";
inline constexpr char CLIENT_INDEX_NAME[] = "CLIENT_INDEX";

// PRIMARY KEYS
inline const TableKey CLIENT_KEY       = {"id",      "INT"};
inline const TableKey INDEX_KEY        = {"id",      "INT"};
inline const TableKey CLIENT_INDEX_KEY = {"cid_iid", "TEXT"}; //derived manually for ease of use

// ATTRIBUTES DEFINITIONS
inline const std::vector<TableKey> CLIENT_ATTRIBUTES = {
    std::make_pair("ip_addr", "TEXT"),
    std::make_pair("port",    "INT") 
};

inline const std::vector<TableKey> INDEX_ATTRIBUTES = {
    std::make_pair("size", "INT"),
};

inline const std::vector<TableKey> CLIENT_INDEX_ATTRIBUTES = {
    std::make_pair("client_id", "INT"),
    std::make_pair("index_id",  "INT")
};

// FOREIGN KEYS
inline const std::vector<ForeignKey> CLIENT_INDEX_FK = {
    std::make_tuple("client_id", "CLIENT", "id"),
    std::make_tuple("index_id",  "INDEX",  "id")
};

} //dfd
