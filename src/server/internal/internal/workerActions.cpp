#include "server/internal/internal/workerActions.hpp"
#include "networking/messageFormatting.hpp"
#include "server/internal/db.hpp"
#include "server/internal/syncing.hpp"
#include <iostream>

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
void serverToServerRegistration(std::vector<uint8_t>&               client_request,
                                std::vector<uint8_t>&               response_dest,
                                std::vector<SourceInfo>&            known_servers,
                                std::mutex&                         knowns_mtx,
                                Database*                           db,
                                std::atomic<bool>&                  record_msgs) {
    
    SourceInfo new_server = parseNewServerReg(client_request);
    //set record_msgs to true
    record_msgs = true;
    //package up db
    if (db->backupDatabase("temp.db") != EXIT_SUCCESS) {
        std::cerr << "db backup into file failed" << std::endl;
    }
    
    response_dest = createServerRegResponse(known_servers);
    
    //DB packup
}

}
