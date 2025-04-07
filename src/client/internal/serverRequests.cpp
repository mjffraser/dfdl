#include "networking/attemptHelpers.hpp"
#include "networking/socket.hpp"
#include "networking/messageFormatting.hpp"
#include "networking/fileParsing.hpp"
#include "networking/internal/fileParsing/fileUtil.hpp"
#include "networking/internal/messageFormatting/byteOrdering.hpp"
#include "sourceInfo.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace dfd {
namespace {

constexpr const char* CONFIG_DIR_NAME  = ".config/dfd";
constexpr const char* UUID_FILE_NAME   = "client.uuid";
constexpr const char* HOSTS_FILE_NAME  = "hosts.txt";

/* Where we will store the uuid + hosts file by default                */
std::filesystem::path configDir()
{
    const char* xdg_cfg = std::getenv("XDG_CONFIG_HOME");
    const char* home    = std::getenv("HOME");
    if (xdg_cfg) return std::filesystem::path(xdg_cfg) / "dfd";
    if (home)    return std::filesystem::path(home)    / CONFIG_DIR_NAME;
    return std::filesystem::current_path() / "dfd_cfg";
}

struct Listener
{
    int         fd  = -1;
    uint16_t    port{0};
    std::mutex  mtx;

    static Listener& instance()
    {
        static Listener inst;
        return inst;
    }

    uint16_t ensureListening()
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (fd >= 0) return port;

        auto sock = openSocket(/*server=*/true, /*port=*/0, /*udp=*/false);
        if (!sock) return 0;
        fd   = sock->first;
        port = sock->second;
        if (tcp::listen(fd, /*max_pending=*/16) != EXIT_SUCCESS)
        {
            closeSocket(fd);
            fd = -1;
            port = 0;
        }
        return port;
    }
};

bool isFail(const std::vector<uint8_t>& msg)     { return !msg.empty() && msg[0] == FAIL; }
bool isOk  (const std::vector<uint8_t>& msg,
            uint8_t expected)                    { return !msg.empty() && msg[0] == expected; }

int sendAndRecv(int                       sock_fd,
                const std::vector<uint8_t>&out,
                std::vector<uint8_t>&      in,
                struct timeval             timeout)
{
    if (tcp::sendMessage(sock_fd, out) != EXIT_SUCCESS)
        return EXIT_FAILURE;

    if (tcp::recvMessage(sock_fd, in, timeout) < 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}


}

int attemptSourceRetrieval(const uint64_t file_uuid,
                           std::vector<SourceInfo> &dest)
{
    std::filesystem::path f_path = file_path;
    if (!std::filesystem::exists(f_path)) // file doesn't exist
    {
        std::cerr << "[attemptIndex] file cannot be found.\n";
        return EXIT_FAILURE;
    }

    uint64_t file_uuid = sha256Hash(f_path);
    if (file_uuid == 0) // failed to build hash
    {
        std::cerr << "[attemptIndex] file hash could not be computed\n";
        return EXIT_FAILURE;
    }

    auto f_size_opt = fileSize(f_path);
    if (!f_size_opt) // failed to get file size
    {
        std::cerr << "[attemptIndex] could not get file size\n";
        return EXIT_FAILURE;
    }
    uint64_t f_size = static_cast<uint64_t>(f_size_opt.value());

    std::filesystem::path cfg = configDir();
    std::filesystem::create_directories(cfg);
    uint64_t my_uuid = getMyUUID((cfg / UUID_FILE_NAME).string());

    SourceInfo me;
    me.peer_id = my_uuid;
    me.ip_addr = getLocalIPAddress();
    me.port    = Listener::instance().ensureListening();

    if (me.ip_addr.empty() || me.port == 0) // could not build self meta
    {
        std::cerr << "[attemptIndex] could not get self ip or port\n";
        return EXIT_FAILURE;
    }

    FileId file_info(file_uuid, me, f_size);
    std::vector<uint8_t> req = createIndexRequest(file_info);
    if (req.empty()) return EXIT_FAILURE;

    auto sock = openSocket(/*server=*/false, /*port=*/0, /*udp=*/false);
    if (!sock) return EXIT_FAILURE;

    if (tcp::connect(sock->first, server, connection_timeout) != EXIT_SUCCESS)
    {
        closeSocket(sock->first);
        return EXIT_FAILURE;
    }

    std::vector<uint8_t> resp;
    if (sendAndRecv(sock->first, req, resp, connection_timeout) != EXIT_SUCCESS)
    {
        closeSocket(sock->first);
        return EXIT_FAILURE;
    }
    closeSocket(sock->first);

    if (!isOk(resp, INDEX_OK) || isFail(resp))
        return EXIT_FAILURE;

    if (std::find(indexed_files.begin(), indexed_files.end(), file_path)
        == indexed_files.end())
        indexed_files.push_back(file_path);

    return EXIT_SUCCESS;
}

int attemptDrop(const std::string&        file_path,
                std::vector<std::string>& indexed_files,
                SourceInfo&               server,
                struct timeval            connection_timeout)
{
    auto it = std::find(indexed_files.begin(), indexed_files.end(), file_path);
    if (it == indexed_files.end()) return EXIT_FAILURE;

    uint64_t file_uuid = sha256Hash(file_path);
    if (file_uuid == 0) return EXIT_FAILURE;

    std::filesystem::path cfg = configDir();
    uint64_t my_uuid = getMyUUID((cfg / UUID_FILE_NAME).string());

    IndexUuidPair ids(file_uuid, my_uuid);
    std::vector<uint8_t> req = createDropRequest(ids);
    if (req.empty()) return EXIT_FAILURE;

    auto sock = openSocket(false, 0, false);
    if (!sock) return EXIT_FAILURE;
    if (tcp::connect(sock->first, server, connection_timeout) != EXIT_SUCCESS)
    {
        closeSocket(sock->first);
        return EXIT_FAILURE;
    }

    std::vector<uint8_t> resp;
    if (sendAndRecv(sock->first, req, resp, connection_timeout) != EXIT_SUCCESS)
    {
        closeSocket(sock->first);
        return EXIT_FAILURE;
    }
    closeSocket(sock->first);

    if (!isOk(resp, DROP_OK) && !isFail(resp))
        return EXIT_FAILURE;      // unexpected opcode

    indexed_files.erase(it);
    return EXIT_SUCCESS;
}


int attemptSourceRetrieval(const uint64_t file_uuid,
                           std::vector<SourceInfo> &dest)
{
    dest.clear();

    std::filesystem::path hosts_file = configDir() / HOSTS_FILE_NAME;
    std::vector<SourceInfo> hosts;
    if (getHostListFromDisk(hosts, hosts_file.string()) != EXIT_SUCCESS
        || hosts.empty())
        return EXIT_FAILURE;

    std::vector<uint8_t> req = createSourceRequest(file_uuid);
    if (req.empty()) return EXIT_FAILURE;

    for (auto& h : hosts)
    {
        auto sock = openSocket(false, 0, false);
        if (!sock) continue;
        struct timeval timeout{3,0};
        if (tcp::connect(sock->first, h, timeout) != EXIT_SUCCESS)
        { closeSocket(sock->first); continue; }

        std::vector<uint8_t> resp;
        if (sendAndRecv(sock->first, req, resp, timeout) != EXIT_SUCCESS)
        { closeSocket(sock->first); continue; }

        closeSocket(sock->first);

        if (isFail(resp))
            continue;
        if (isOk(resp, SOURCE_LIST))
        {
            dest = parseSourceList(resp);
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

} // namespace dfd
