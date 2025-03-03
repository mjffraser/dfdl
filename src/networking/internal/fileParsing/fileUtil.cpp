#include "networking/internal/fileParsing/fileUtil.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace dfd {

ssize_t bytesInFile(const std::filesystem::path& f_path) {
    if (!std::filesystem::exists(f_path))
        return -1;
    return std::filesystem::file_size(f_path);
}

std::optional<ssize_t> readFile(const std::filesystem::path& f_path,
                                const size_t                 read_size,
                                const size_t                 offset,
                                      std::vector<uint8_t>&  buff) {
    if (!std::filesystem::exists(f_path))
        return std::nullopt;

    //need space to write the data, otherwise read on .data() is undefined
    if (buff.size() < read_size)
        buff.resize(read_size);

    std::ifstream file(f_path, std::ios::binary);
    if (!file)
        return std::nullopt;

    file.seekg(offset);
    if (!file)
        return std::nullopt;

    file.read(reinterpret_cast<char*>(buff.data()), read_size);
    if (file.gcount() < read_size)
        buff.resize(file.gcount());
    return file.gcount(); //file deallocated when stack frame is popped
}

std::unique_ptr<std::ofstream> writeToNewFile(const std::filesystem::path& f_path,
                                              const size_t                 len,
                                              const std::vector<uint8_t>&  data) {
    if (std::filesystem::exists(f_path)) {
        return nullptr; //file exists, do not overwrite
    }

    auto file = std::make_unique<std::ofstream>(f_path, std::ios::binary);
    if (!file->is_open())
        return nullptr;

    file->write(reinterpret_cast<const char*>(data.data()), len);

    if (!file->good())
        return nullptr;
    
    file->flush();
    return file;
}

int writeToFile(      std::ofstream*         file,
                const size_t                 len,
                const std::vector<uint8_t>&  data,
                const size_t                 offset) {
    if (!file->is_open())
        return EXIT_FAILURE;

    file->seekp(offset);
    file->write(reinterpret_cast<const char*>(data.data()), len);
    file->flush();
    return EXIT_SUCCESS;
}


int deleteFile(const std::filesystem::path& f_path) {
    if (!std::filesystem::exists(f_path))
        return EXIT_FAILURE;
    auto ret = std::filesystem::remove(f_path);
    if (ret)
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}

} //dfd
