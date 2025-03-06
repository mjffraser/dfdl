#pragma once

#include <bits/types/struct_timeval.h>
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

namespace dfd {

struct SourceInfo;

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * msgLenToBytes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Convert an integer into byte array. 
 *
 * Takes:
 * -> val:
 *    The Interger to be converted.
 * -> buffer:
 *    The container to store the converted bytes to.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void msgLenToBytes(const size_t val, uint8_t* buffer);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * bytesToSize_t
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Convert a byte array into an integer.
 *
 * Takes:
 * -> buffer:
 *    The bytes vector to be converted.
 *
 * Returns:
 * -> val:
 *    The converted integer
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
size_t bytesToMsgLen(const std::vector<uint8_t>& buffer);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * recvBytes
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads bytes on the socket into a buffer if it's present. 
 *
 * Takes:
 * -> socket_fd:
 *    The socket to read the data from.
 * -> buffer:
 *    The container to append the read bytes to.
 * -> try_to_recv:
 *    Bytes attempt to read.
 * -> timeout:
 *    How long this function attempts to read try_to_recv bytes for before
 *    giving up. 
 *
 * Returns:
 * -> On success:
 *    The number of bytes read. May or may not be equal to try_to_recv.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
ssize_t recvBytes(int                  socket_fd, 
                 std::vector<uint8_t>& buffer, 
                 size_t                try_to_recv, 
                 timeval               timeout);

} //dfd
