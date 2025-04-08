#include "server/internal/internal/workerActions.hpp"
#include "networking/messageFormatting.hpp"
#include "server/internal/db.hpp"
#include "server/internal/syncing.hpp"

namespace dfd {

void clientIndexRequest(const std::vector<uint8_t>& client_request,
                              std::vector<uint8_t>& response_dest,
                              Database*             db) {
    FileId file_id = parseIndexRequest(client_request);
    if (file_id.uuid == 0) {
        response_dest = createFailMessage("Insufficient file data provided.");
        return;
    }

    if (EXIT_SUCCESS != db->indexFile(file_id.uuid,
                                      file_id.indexer,
                                      file_id.f_size))
        response_dest = createFailMessage(db->sqliteError());
    else
        response_dest = {INDEX_OK};
}

void clientDropRequest(const std::vector<uint8_t>& client_request,
                             std::vector<uint8_t>& response_dest,
                             Database*             db) {
    IndexUuidPair uuids = parseDropRequest(client_request);
    if (uuids.first == 0 || uuids.second == 0) {
        response_dest = createFailMessage("Either or both of the received UUID's are malformed.");
        return;
    }

    if (EXIT_SUCCESS != db->dropIndex(uuids.first, uuids.second))
        response_dest = createFailMessage(db->sqliteError());
    else
        response_dest = {DROP_OK};
}

void clientReregisterRequest(const std::vector<uint8_t>& client_request,
                                   std::vector<uint8_t>& response_dest,
                                   Database*             db) {
    SourceInfo client_info = parseReregisterRequest(client_request);
    if (client_info.port == 0) {
        response_dest = createFailMessage("Insufficient address data provided.");
        return;
    }

    if (EXIT_SUCCESS != db->updateClient(client_info))
        response_dest = createFailMessage(db->sqliteError());
    else
        response_dest = {REREGISTER_OK};
}

void clientSourceRequest(const std::vector<uint8_t>& client_request,
                               std::vector<uint8_t>& response_dest,
                               Database*             db) {
    uint64_t f_uuid = parseSourceRequest(client_request);
    if (f_uuid == 0) {
        response_dest = createFailMessage("Invalid file uuid provided.");
        return;
    }
    
    std::vector<SourceInfo> indexers;
    if (EXIT_SUCCESS != db->grabSources(f_uuid, indexers))
        response_dest = createFailMessage(db->sqliteError());
    else
        response_dest = createSourceList(indexers);
}

//NOTE: no const on client request could mabye cause a issue
void clientServerRegistration(std::vector<uint8_t>&      client_request,
                                std::vector<uint8_t>&    response_dest,
                                std::vector<SourceInfo>& known_servers) {
    
    SourceInfo new_server = parseNewServerReg(client_request);
    ssize_t registered_with = forwardRegistration(client_request, known_servers);
    if (registered_with < 0) {
        response_dest = createFailMessage("I appear to be a dead server myself?");
    } else {
        response_dest = createServerRegResponse(known_servers);
        known_servers.push_back(new_server);
        //currently unused
        //db->backupDatabase("temp.db");
    }  
}

}
