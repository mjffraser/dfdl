#pragma once

#include "sourceInfo.hpp"
#include <string>
#include <vector>

namespace dfd {

int doIndex(const SourceInfo&               my_listener,
            const std::string&              file_path,
                  std::vector<std::string>& indexed_files,
                  std::vector<SourceInfo>&  server_list);

int doDrop(const SourceInfo&               my_listener,
           const std::string&              file_path,
                 std::vector<std::string>& indexed_files,
                 std::vector<SourceInfo>&  server_list);

int doDownload();

}
