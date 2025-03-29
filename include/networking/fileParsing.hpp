#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * RECOMMENDED USAGE:
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Downloader:
 * -> with the file size, call fileChunks() to get the number of chunks to recv.
 * -> recv data into buffer and call unpackFileChunk() to store partial chunks
 *    to the disk so others can be recieved async.
 * -> when you have chunk 0 unpacked, call openFile(), which will convert
 *    the first chunk to a file pointer that can be written to. this is NOT
 *    thread safe. one thread should be calling fileSize() periodically to check
 *    for file chunks.
 * -> with these chunks, call assembleChunk() to move the chunk data into the
 *    file.
 * -> when all chunks written, destroy the file pointer to save and write to
 *    disk. use std::move() to give the pointer away for cleanup.
 *
 * Sender:
 * -> Call fileSize() to get file size. Call fileChunks() for number of chunks
 *    to send.
 * -> For chunks 0..n call packageFileChunk() to read them into memory to send.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * A note on the default download directories
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * There are three attempts made to set a download directory. They are as
 * follows:
 * -> The environment variable $XDG_DOWNLOAD_DIR
 *    This is almost certainly not set on the system. TODO, change to read
 *    user-dirs
 * -> ~/dfd
 *    A directory named dfd inside $HOME. This should work on most machines
 *    configured to be usable.
 * -> cwd
 *    The current working directory. At least it exists.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * setChunkSize
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Sets the chunk size to send and recieve at a time. The default of 1MiB is
 *    recommended (ie, don't call this unless you have a good reason to).
 *
 * Takes:
 * -> size:
 *    The size of a chunk, in bytes. If 0, nothing happens.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void setChunkSize(const size_t size);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * setDownloadDir
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Set the directory to use for downloading to. Attempts to create the
 *    directory(ies). If directories existance cannot be verified either before
 *    or after the attempted creation a failure is returned.
 *
 * Takes:
 * -> f_path:
 *    The path.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int setDownloadDir(const std::filesystem::path& f_path); 

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
std::optional<ssize_t> fileSize(const std::filesystem::path& f_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * chunksInFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Returns the number of chunks the file will be, calculated with the
 *    internal chunk_size.
 *
 * Takes:
 * -> f_size:
 *    The size of the file, obtained from fileSize().
 *
 * Returns:
 * -> On success:
 *    Number of chunks.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<size_t> fileChunks(const ssize_t f_size);

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
 *    Bytes read into buffer.
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<ssize_t> packageFileChunk(const std::filesystem::path& f_path,
                                              std::vector<uint8_t>&  buff,
                                        const size_t                 chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * storeFileChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Takes a recieved chunk of bytes, some data recieved from a collection of
 *    packets. Stores the bytes to disk so we can reuse the RAM. Stores the file
 *    in the download directory as f_path->[chunk#]
 *
 * Takes:
 * -> f_name:
 *    The name of the file where building. Ex. doc.txt, and chunks like
 *    doc.txt-0 are unpacked.
 * -> buff:
 *    The buffer to read data off of.
 * -> data_len:
 *    The length of data to write.
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
int unpackFileChunk(const std::string&           f_name,
                    const std::vector<uint8_t>&  buff,
                    const size_t                 data_len,
                    const size_t                 chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * openFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a file, writing the first chunk to it. Must have the first chunk
 *    unpacked by any download process to start creating the main file. Doesn't
 *    care if first chunk is less than chunk_size (already EOF), expects the
 *    caller to handle calling saveFile() after. Errors out if first chunk is
 *    missing, or for any other reason like not being able to write to disk.
 *
 * Takes:
 * -> f_name:
 *    The name of the file to create inside the download directory.
 *
 * Returns:
 * -> On success:
 *    A pointer to the file.
 * -> On failure:
 *   nullptr
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::unique_ptr<std::ofstream> openFile(const std::string& f_name);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * assembleChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Appends a chunk to the file.
 *
 * Takes:
 * -> file:
 *    The file to write the chunk to.
 * -> chunk:
 *    The size of the entire file.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int assembleChunk(      std::ofstream* file,
                  const std::string&                    f_name,
                  const size_t                          chunk);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * saveFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Calls close on file, and releases ownership. Should only be called once
 *    all chunks have been written to the file with assembleChunk.
 *
 * Takes:
 * -> file:
 *    The file to close and deallocate.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int saveFile(std::unique_ptr<std::ofstream> file);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * deleteFile
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Deletes a file, if it exists.
 *
 * Takes:
 * -> f_path:
 *    The path to the file to delete.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int deleteFile(const std::string& f_path);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * sha512Hash
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Computes the SHA512 hash of a file on the disk. If the file DNE, or on any
 *    other error (perms, etc.) 0 is returned. Truncates the first 8-bytes to
 *    return.
 *
 * Takes:
 * -> f_path:
 *    The path to the file, relative or absolute, on the disk.
 *
 * Returns:
 * -> On success:
 *    A 8-byte hash.
 * -> On failure:
 *    0
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
uint64_t sha256Hash(const std::filesystem::path& f_path);

}
