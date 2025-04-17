#pragma once

#include "sourceInfo.hpp"
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * getHostListFromDisk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Read a hosts file from the disk and store it in the provided vector.
 *
 * Takes:
 * -> dest:
 *    The vector to store these in. NOTE: vector is cleared by this function.
 * -> host_path:
 *    The path to the file to read from.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int getHostListFromDisk(std::vector<SourceInfo>& dest, const std::string& host_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * storeHostListToDisk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Stores all the hosts in the provided vector into a file in a format that
 *    can be parsed by the above function. NOTE: this clears the file prior to
 *    writing.
 *
 * Takes:
 * -> hosts:
 *    The vector of hosts to store.
 * -> host_path:
 *    The path of the file to write to, or create and write to.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int storeHostListToDisk(const std::vector<SourceInfo>& hosts, const std::string& host_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * getMyUUID
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads in a uuid from the disk that's stored in a file. If the file doesn't
 *    exist, generates a UUID from dev/urandom, stores it to the disk at the
 *    provided path, then returns the generated UUID.
 *
 * Takes:
 * -> uuid_path:
 *    The path to read from. If no file exists, or the file doesn't contain
 *    exactly 8-bytes of data, regenerages a UUID, and stores to this path.
 *
 * Returns:
 * -> On success:
 *    The UUID.
 * -> On failure:
 *    0
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
uint64_t getMyUUID(const std::string& uuid_path);

} //dfd
