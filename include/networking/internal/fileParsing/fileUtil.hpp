#pragma once

#include <filesystem>
#include <optional>
#include <sys/types.h>
#include <vector>
#include <fstream>
#include <memory>

// THESE FUNCTIONS MAKE ZERO EFFORT TO HANDLE CONCURRENCY.
// IF WRITE FUNCTIONS ARE USED BY MULTIPLE THREADS BEHAVIOUR
// IS COMPLETELY UNDEFINED.

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * bytesInFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Returns the size of a file, in bytes.
 *
 * Takes:
 * -> f_path:
 *    The path to the file.
 *
 * Returns:
 * -> On success:
 *    The size.
 * -> On failure:
 *    -1
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
ssize_t bytesInFile(const std::filesystem::path& f_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * readFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a file, from a specific offset byte (0-indexed), for read_size
 *    bytes, or when it reaches EOF, whichever is first. Stores the bytes in
 *    buff.
 *
 * Takes:
 * -> f_path:
 *    The path to the file.
 * -> read_size:
 *    The number of bytes to read. Reading less than read_size means EOF.
 * -> offset:
 *    Where to start reading from. 0 for start of file.
 * -> buff:
 *    Where to store the read bytes. Clears the vector prior to reading. Vector
 *    should ideally have memory allocated prior to calling this.
 *
 * Returns:
 * -> On success:
 *    Bytes read.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<ssize_t> readFile(const std::filesystem::path& f_path,
                                const size_t                 read_size,
                                const size_t                 offset,
                                      std::vector<uint8_t>&  buff);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * writeToNewFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a new file if none exists, and writes data to it. If a file exists
 *    it's considered a failure. 
 *
 * Takes:
 * -> f_path:
 *    The path of the file to create.
 * -> len:
 *    The length of the data. Depending on resizes, the length of data might be
 *    different.
 * -> data:
 *    The bytes to write.
 *
 * Returns:
 * -> On success:
 *    The ofstream of the file.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::unique_ptr<std::ofstream> writeToNewFile(const std::filesystem::path& f_path,
                                              const size_t                 len,
                                              const std::vector<uint8_t>&  data);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * writetoFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Writes data to an offset in a file. If the provided offset is farther than
 *    the end of the file, writes NUL bytes to extend the file. If file doesn't
 *    exist, returns with an error.
 *
 * Takes:
 * -> file:
 *    The file to append to.
 * -> len:
 *    The length of data.
 * -> data:
 *    The data to write.
 * -> offset:
 *    The offset to write at.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int writeToFile(      std::ofstream*         file,
                const size_t                 len,
                const std::vector<uint8_t>&  data,
                const size_t                 offset);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * deleteFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Deletes the file at f_path. If no file exists to delete, returns with an
 *    error.
 *
 * Takes:
 * -> f_path:
 *    The file to delete.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int deleteFile(const std::filesystem::path& f_path);

} //dfd

