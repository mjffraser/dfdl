#pragma once

#include "sourceInfo.hpp"
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doIndex
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Indexes a file, trying every server in server_list if needed. Appends the
 *    absolute path of the indexed file to indexed_files on success. On failure,
 *    an error is printed by this function. May modify server_list if this
 *    operation finds it necessary to get an updated server_list from servers.
 *    This usually occurs due to servers not responding, and will occur
 *    following successful recalibration of the server list OR if one server is
 *    identified as faulty, while others respond. If this function returns an
 *    error it's indicitive of failure of every server provided. In that case,
 *    server_list is not modified, and the error is returned instead.
 *
 * Takes:
 * -> my_listener:
 *    A SourceInfo object that houses all of this client's info. All fields
 *    MUST be set.
 * -> file_path:
 *    A path to the file, relative or absolute.
 * -> indexed_files:
 *    The vector of indexed_files that client_main() maintains.
 * -> server_list:
 *    The list of servers that client.cpp maintains.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int doIndex(const SourceInfo&                      my_listener,
            const std::string&                     file_path,
                  std::map<uint64_t, std::string>& indexed_files,
                  std::mutex&                      indexed_files_mtx,
                  std::vector<SourceInfo>&         server_list);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doDrop 
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Drops a file, trying every server in server_list if needed. Erases the
 *    absolute path of the indexed file from indexed_files on success. On
 *    failure, an error is printed by this function. May modify server_list if
 *    this operation finds it necessary to get an updated server_list from
 *    servers. This usually occurs due to servers not responding, and will occur
 *    following successful recalibration of the server list OR if one server is
 *    identified as faulty, while others respond. If this function returns an
 *    error it's indicitive of failure of every server provided. In that case,
 *    server_list is not modified, and the error is returned instead.
 *
 * Takes:
 * -> my_listener:
 *    A SourceInfo object that houses all of this client's info. All fields
 *    MUST be set.
 * -> file_path:
 *    A path to the file, relative or absolute.
 * -> indexed_files:
 *    The vector of indexed_files that client_main() maintains.
 * -> server_list:
 *    The list of servers that client.cpp maintains.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int doDrop(const SourceInfo&                      my_listener,
           const std::string&                     file_path,
                 std::map<uint64_t, std::string>& indexed_files,
                 std::mutex&                      indexed_files_mtx,
                 std::vector<SourceInfo>&         server_list);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doDownload
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Downloads a file from other peers, trying every server in the server_list
 *    if needed.  
 *
 *    May modify server_list if this operation finds it necessary to get an
 *    updated server_list from servers. This usually occurs due to servers not
 *    responding, and will occur following successful recalibration of the
 *    server list OR if one server is identified as faulty, while others
 *    respond. If this function returns an error it's indicitive of failure of
 *    every server provided. In that case, server_list is not modified, and the
 *    error is returned instead.
 *
 *    Manages communicating faulty peers to the server entirely internally.
 *    
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int doDownload(const uint64_t                 f_uuid,
                     std::vector<SourceInfo>& server_list);

}
