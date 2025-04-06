#pragma once 

#include "sourceInfo.hpp"
#include <cstdint>
#include <vector>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * attemptFileDownload
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Opens and manages a pool of threads to request file chunks from the list
 *    of peers provided. If bad peers are detected in the list they are removed
 *    from the sources list after the download process concludes, and are added
 *    to bad_sources. Anything inside bad_sources is cleared by this function
 *    before populating, so a size() of 0 indicates no bad peers detected.
 *
 *    This modification only happens at the very end. In the event of a failure,
 *    bad_sources will be identical to what sources was initially, and
 *    sources will be empty.
 *
 * Takes:
 * -> file_uuid:
 *    The UUId of the file to attempt to download.
 * -> sources:
 *    The list of peers to try downloading from. Bad sources are removed from
 *    this vector post-download.
 * -> bad_sources:
 *    The vector to store all bad sources in post-download.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int attemptFileDownload(const uint64_t file_uuid,
                        std::vector<SourceInfo>& sources,
                        std::vector<SourceInfo>& bad_sources);

}
