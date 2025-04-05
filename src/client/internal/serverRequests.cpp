#include "client/internal/serverRequests.hpp"
namespace dfd
{
    int attemptIndex(const std::string &file_path,
                     std::vector<std::string> &indexed_files,
                     SourceInfo &server,
                     struct timeval connection_timeout)
    {
    }

    int attemptDrop(const std::string &file_path,
                    std::vector<std::string> &indexed_files,
                    SourceInfo &server,
                    struct timeval connection_timeout)
    {
    }

    int attemptSourceRetrieval(const uint64_t file_uuid,
                               std::vector<SourceInfo> &dest)
    {
    }
}