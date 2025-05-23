#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace dfd {

//format that rows from the table are returned as
using Row = std::vector<std::string>;

//Primary key for a table
using TableKey = std::pair<std::string,  //KEY_NAME
                           std::string>; //KEY_TYPE

//Foreign key for a table
using ForeignKey = std::tuple<std::string,  //KEY_NAME
                              std::string,  //KEY_TYPE
                              std::string>; //REF_KEY

using AttributeValuePair = std::pair<std::string,                //attribute name
                                     std::variant<uint64_t,      //possible type (64-bit hashes, file sizes)
                                                  uint16_t,      //possible type (16-bit port #)
                                                  std::string>>; //possible type (names, etc.)
} //dfd
