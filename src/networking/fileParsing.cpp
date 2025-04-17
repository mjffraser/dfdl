#include "networking/fileParsing.hpp"
#include "networking/internal/fileParsing/fileUtil.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <openssl/evp.h>

namespace dfd {

std::filesystem::path initDownloadDir() {
    const char* xdg_download_dir = std::getenv("XDG_DOWNLOAD_DIR");
    const char* home             = std::getenv("HOME");
    std::filesystem::path file_path;

    if (xdg_download_dir)
        file_path = std::filesystem::path(xdg_download_dir) / "dfd";
    else if (home)
        file_path = std::filesystem::path(home) / "dfd";
    else
        file_path = std::filesystem::current_path();

    return file_path;
}

//DEFAULT: 1Mib, or 1024*1024 bytes.
static size_t chunk_size = 1 << 20;
static std::filesystem::path download_path = initDownloadDir(); 

int setDownloadDir(const std::filesystem::path& f_path) {
    if (!std::filesystem::exists(f_path)) {
        try {
            if (!std::filesystem::create_directories(f_path))
                return EXIT_FAILURE;
        } catch (...) {
            return EXIT_FAILURE;
        }
    }

    //if all good, dir exists
    download_path = f_path;
    return EXIT_SUCCESS;
}

std::filesystem::path getDownloadDir() {
    return download_path;
}

void setChunkSize(const size_t size) {
    if (size != 0)
        chunk_size = size;
}

std::optional<ssize_t> fileSize(const std::filesystem::path& f_path) {
    ssize_t size = bytesInFile(f_path);
    if (size < 0)
        return std::nullopt;
    return size;
}

std::optional<size_t> fileChunks(const ssize_t f_size) {
    if (chunk_size < 1 || f_size < 1)
        return std::nullopt;
    
    return std::ceil(double(f_size) / double(chunk_size));
}

std::optional<ssize_t> packageFileChunk(const std::filesystem::path& f_path, 
                                              std::vector<uint8_t>&  buff, 
                                        const size_t                 chunk) {
    auto f_size = fileSize(f_path);  
    if (!f_size)
        return std::nullopt; //can't find file
    
    if (f_size.value() == 0 && chunk == 0)
        return 0; //empty file

    auto f_chunks = fileChunks(f_size.value());
    if (!f_chunks)
        return std::nullopt; //shouldn't happen

    if (chunk > f_chunks.value()-1)
        return std::nullopt; //reading past EOF
    
    size_t offset = chunk*chunk_size;
    auto read_bytes = readFile(f_path, chunk_size, offset, buff);
    if (read_bytes) {
        if (read_bytes.value() <= chunk_size &&
            read_bytes.value() >= 0)
            return read_bytes.value();
    }

    return std::nullopt;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * filePath
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Constructs a path to a chunk file.
 *
 * Takes:
 * -> f_name:
 *    The name of the base file.
 * -> chunk:
 *    The chunk number, 0-indexed, if a chunk.
 * -> is_chunk:
 *    If generating path to the general file, or a chunk.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::filesystem::path filePath(const std::string& f_name, 
                                const size_t       chunk,
                                      bool         is_chunk) {
    std::string c_name = f_name;
    if (is_chunk)
        c_name += "->" + std::to_string(chunk);
    return std::filesystem::path(download_path / c_name);
}

int unpackFileChunk(const std::string&           f_name, 
                    const std::vector<uint8_t>&  buff, 
                    const size_t                 data_len,
                    const size_t                 chunk) {
    std::string c_name = filePath(f_name, chunk, true); 
    std::filesystem::path c_path = download_path / c_name;
    if (!std::filesystem::exists(download_path) ||
        !std::filesystem::is_directory(download_path))
        if (EXIT_SUCCESS != setDownloadDir(download_path))
            return EXIT_FAILURE;

    auto file = writeToNewFile(c_path, data_len, buff); 
    if (!file)
        return EXIT_FAILURE;
    file.reset(); //release
    return EXIT_SUCCESS;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * verifyChunk
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Verifies that a chunk file exists and can be read.
 *
 * Takes:
 * -> c_path:
 *    The path to the chunk, obtained from filePath().
 *
 * Returns:
 * -> On success:
 *    true
 * -> On failure:
 *    false
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
bool verifyChunk(const std::filesystem::path& c_path) {  
    if (!std::filesystem::exists(c_path))
        return false;
    return true;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * readChunkData
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Reads a chunk of data into a buffer so it can be written elsewhere.
 *
 * Takes:
 * -> c_path:
 *    The path to the chunk, obtained from filePath().
 *
 * Returns:
 * -> On success:
 *    Pair <data_len, data> 
 * -> On failure:
 *    std::nullopt
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::pair<size_t, std::vector<uint8_t>>> readChunkData(const std::filesystem::path& c_path) {
    if (!verifyChunk(c_path))
        return std::nullopt;

    std::vector<uint8_t> c_data;
    auto data_size = fileSize(c_path);
    if (!data_size)
        return std::nullopt;

    auto read_bytes = readFile(c_path,
                               static_cast<size_t>(data_size.value()),
                               0,
                               c_data);

    if (!read_bytes)
        return std::nullopt; //couldn't read in chunk data

    if (read_bytes.value() != data_size.value())
        return std::nullopt; //didn't read entire file
    
    return std::make_pair(read_bytes.value(), c_data);
}

std::unique_ptr<std::ofstream> openFile(const std::string& f_name) {
    auto c_path = filePath(f_name, 0, true);
    auto f_path = filePath(f_name, 0, false); 
    auto c_pair = readChunkData(c_path);
    if (!c_pair)
        return nullptr;

    auto& [c_len, c_data] = *c_pair;
    auto file = writeToNewFile(f_path, c_len, c_data);

    if (file)
        deleteFile(c_path); //chunk is written
    return file;
}

int assembleChunk(      std::ofstream* file, 
                  const std::string&                    f_name,
                  const size_t                          chunk) {
    auto c_path = filePath(f_name, chunk, true);
    auto c_pair = readChunkData(c_path); 
    if (!c_pair)
        return EXIT_FAILURE;

    auto& [c_len, c_data] = *c_pair;
    size_t offset = chunk_size*chunk;
    if (EXIT_SUCCESS != writeToFile(file, c_len, c_data, offset))
        return EXIT_FAILURE;
    deleteFile(c_path); //chunk is written
    return EXIT_SUCCESS;
}

int saveFile(std::unique_ptr<std::ofstream> file) {
    file->close();
    return EXIT_SUCCESS;
}

int deleteFile(const std::string& f_path) {
    if (!std::filesystem::exists(f_path))
        return EXIT_FAILURE;
    std::filesystem::remove(f_path);
    if (std::filesystem::exists(f_path))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

union DigestMap {
    uint64_t uuid;
    uint8_t  digest[8];
};

uint64_t sha256Hash(const std::filesystem::path& f_path) {
    auto f_size = fileSize(f_path);     
    if (!f_size)
        return 0;

    auto chunks = fileChunks(f_size.value());
    if (!chunks) 
        return 0;

	EVP_MD_CTX* mdctx;
	if ((mdctx = EVP_MD_CTX_new()) == NULL)
        return 0;

	if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL))
        return 0;

    std::vector<uint8_t> buff; buff.resize(chunk_size);
    for (size_t i = 0; i < chunks.value(); ++i) {
        auto res = packageFileChunk(f_path, buff, i);        
        if (!res)
            return 0;
        if (1 != EVP_DigestUpdate(mdctx, buff.data(), res.value()))
            return 0;
    }

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
	if (1 != EVP_DigestFinal_ex(mdctx, digest, &hash_len))
        return 0;

	EVP_MD_CTX_free(mdctx);
    
    DigestMap dm;
    std::memcpy(&dm, digest, sizeof(uint64_t));

    //we need to make sure big and little endian machines return
    //the hash in the same ordering, so they handle it the same
    int err_code = 0;
    dm.uuid = toNetworkOrder(dm.uuid, err_code);

    return dm.uuid;
}

} //dfd
