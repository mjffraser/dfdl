#pragma once

#include <cstdint>
#include <string>
namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * toNetworkOrder
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Converts a fixed-width integer representation into big-endian, regardless
 *    of host ordering.
 *
 * Takes:
 * -> host_data:
 *    One of:
 *    -> uint16_t
 *    -> uint32_t
 *    -> uint64_t
 * -> err_flag:
 *    A reference to an integer. If an error occurs, this integer is set to 1.
 *
 * Returns:
 * -> On success:
 *    A big endian version of the input.
 * -> On failure:
 *    Return is undefined. err_flag will be set.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
template <typename T>
T toNetworkOrder(T host_data, int& err_flag) {
    auto ordered = (sizeof(T) == sizeof(uint16_t)) ? htobe16(host_data) :
                   (sizeof(T) == sizeof(uint32_t)) ? htobe32(host_data) :
                   (sizeof(T) == sizeof(uint64_t)) ? htobe64(host_data) :
                   -1;
    if (ordered == -1)
        err_flag = 1;
    return ordered; 
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * fromNetworkOrder
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reverses the above operation. Takes a big-endian fixed-width integer
 *    representation and converts it to the host-machine's ordering.
 *
 * Takes:
 * -> network_data:
 *    One of:
 *    -> uint16_t
 *    -> uint32_t
 *    -> uint64_t
 * -> err_flag:
 *    A reference to an integer. If an error occurs, this integer is set to 1.
 * -> On failure:
 *    Return is undefined. err_flag will be set.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
template <typename T> 
T fromNetworkOrder(T network_data, int& err_flag) {
    auto ordered = (sizeof(T) == sizeof(uint16_t)) ? be16toh(network_data) :
                   (sizeof(T) == sizeof(uint32_t)) ? be32toh(network_data) :
                   (sizeof(T) == sizeof(uint64_t)) ? be64toh(network_data) :
                   -1;
    if (ordered == -1)
        err_flag = 1;
    return ordered;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * getIpBytes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a string representation of an IPv4 address (xxx.xxx.xxx.xxx), and
 *    converts it to a uint32_t in big-endian network ordering.
 *
 * Takes:
 * -> ip_str:
 *    The IPv4 address.
 *
 * Returns:
 * -> On success:
 *    The network ordered representation.
 * -> On failure:
 *    0
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
uint32_t getIpBytes(const std::string& ip_str);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * ipBytesToString
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reverses the above, returning a string representation of the network-
 *    ordered IPv4 address.
 *
 * -> ip_bytes:
 *    A pointer to the start of the byte sequence that was recieved. 4-bytes
 *    will be read from this point. It's up to the caller to make sure this is
 *    defined.
 * 
 * Returns:
 * -> On success:
 *    The string address.
 * -> On failure:
 *    An empty string.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::string ipBytesToString(const uint8_t* ip_bytes);

} //dfd
