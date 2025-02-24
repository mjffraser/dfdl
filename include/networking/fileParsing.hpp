#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * fileSize
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Returns the size of a file in bytes.
 *
 * Takes:
 * -> f_path:
 *    The path to the file, absolute or relative from cwd.
 *
 * Returns:
 * -> On success:
 *    A non-negative number of bytes.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<ssize_t> fileSize(const std::string& f_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * packageFileChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a file, putting the result into the provided buffer. Reads the file
 *    from the provided file path, absolute or relative to the cwd.
 *
 * Takes:
 * -> file_path:
 *    The path to the file, absolute from root or relative from cwd.
 * -> buff:
 *    The buffer to read the file into. Any data in the buffer prior to this
 *    will be overwritten. chunk_size bytes will be written here, unless it's
 *    the final chunk. It could be anything between 1 and chunk_size in that 
 *    case.
 * -> chunk_size:
 *    The size of file chunks. 
 * -> chunk:
 *    Which chunk to read in. 0-indexed.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int packageFileChunk(const std::string&      file_path, 
                     std::vector<uint8_t>&   buff, 
                     const size_t            chunk_size,
                     const size_t            chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * storeFileChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a recieved chunk of bytes, some data recieved from a collection of
 *    packets. Stores the bytes to disk so we can reuse the RAM. Temp files are
 *    stored in the cwd, then deleted by assembleFile.
 *
 * Takes:
 * -> file_path:
 *    The path to the file, absolute from root or relative from cwd.
 * -> buff:
 *    The buffer to read the file into. Any data in the buffer prior to this
 *    will be overwritten.
 * -> chunk:
 *    Which chunk to store. 0-indexed.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int unpackFileChunk(const std::string&          file_path, 
                    const std::vector<uint8_t>& buff, 
                    const size_t                chunk);

/* 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * assembleFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Combines all file chunks into a single file. If this function can't locate
 *    all the file chunks and assert that it has f_size bytes stored it'll fail.
 *
 * Takes:
 * -> file_path:
 *    The path, absolute or relative, to store the final file at.
 * -> f_size:
 *    The size of the entire file.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int assembleFile(const std::string& file_path, const size_t f_size);

}
