#include "server/internal/database/db.hpp"
#include "server/internal/database/internal/types.hpp"
#include "server/internal/database/internal/queries.hpp"
#include "server/internal/database/internal/tableInfo.hpp"
#include "sourceInfo.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace dfd {

std::string Database::sqliteError() {
    return err_msg;
}

int Database::reportError(const std::string& e) {
    err_msg = e;
    return EXIT_FAILURE;
}

int Database::setupDatabase() {
    std::vector<ForeignKey> no_fk; //for tables without foreign keys

    //create tables
    auto err_val = createTable(db, 
                               PEER_NAME, 
                               PEER_KEY,
                               no_fk,
                               PEER_ATTRIBUTES);
    if (err_val)
        return reportError(err_val.value());

    err_val = createTable(db,
                          FILE_NAME,
                          FILE_KEY,
                          no_fk,
                          FILE_ATTRIBUTES);
    if (err_val)
        return reportError(err_val.value());

    err_val = createTable(db,
                          INDEX_NAME,
                          INDEX_KEY,
                          INDEX_FK,
                          INDEX_ATTRIBUTES);
    if (err_val)
        return reportError(err_val.value());

    return EXIT_SUCCESS;
}

int Database::insertOrUpdate(sqlite3*                               db, 
                             const std::string&                     table_name, 
                             const AttributeValuePair&              pk_pair, 
                                   std::vector<AttributeValuePair>& values,
                             const std::string&                     pk_condition) {
    std::vector<Row> temp; 
    auto err_val = doSelect(db, table_name, {pk_pair.first}, {pk_condition}, &temp);
    if (err_val)
        return reportError(err_val.value());
    
    if (temp.size() == 0) {
        //no entry, so do insert
        values.push_back(pk_pair); //add primary key value into values
        err_val = doInsert(db, table_name, values);
        if (err_val)
            return reportError(err_val.value());

    } else {
        //otherwise, we just update stuff
        err_val = doUpdate(db, table_name, pk_pair, values);
        if (err_val)
            return reportError(err_val.value());
    }

    return EXIT_SUCCESS;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * compositeKey
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Given a SourceInfo object, and a file uuid, returns the composite key used
 *    in the INDEX table.
 *
 * Takes:
 * -> indexer:
 *    SourceInfo object of client in PEERS table.
 * -> uuid:
 *    The uuid of the file in the FILES table.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::string indexKey(const SourceInfo& indexer, const uint64_t uuid) {
    return std::to_string(indexer.peer_id) + "|" + std::to_string(uuid);
}

int Database::indexFile(const uint64_t     uuid, 
                        const SourceInfo&  indexer,
                        const uint64_t     f_size) {
    //first check if indexer already exists and up-to-date
    std::string        peer_condition = PEER_KEY.first + "=" + std::to_string(indexer.peer_id);
    AttributeValuePair peer_pk        = std::make_pair(PEER_KEY.first, indexer.peer_id);
    std::vector<AttributeValuePair> peer_values = {
        {PEER_ATTRIBUTES[0].first, indexer.ip_addr},
        {PEER_ATTRIBUTES[1].first, indexer.port}
    };

   int res = insertOrUpdate(db, 
                            PEER_NAME, 
                            peer_pk, 
                            peer_values, 
                            peer_condition);
    if (res == EXIT_FAILURE)
        return EXIT_FAILURE; 

    //now we need to check if the file to index already exists
    std::string        file_condition = FILE_KEY.first + "=" + std::to_string(uuid);
    AttributeValuePair file_pk        = std::make_pair(FILE_KEY.first, uuid);
    std::vector<AttributeValuePair> file_values = {
        {FILE_ATTRIBUTES[0].first, f_size}
    };

    res = insertOrUpdate(db, 
                         FILE_NAME, 
                         file_pk, 
                         file_values, 
                         file_condition);
    if (res == EXIT_FAILURE)
        return EXIT_FAILURE;

    //finally, associate the indexer with the file
    std::string        index_key       = indexKey(indexer, uuid);
    std::string        index_condition = INDEX_KEY.first + "='" + index_key + "'";
    AttributeValuePair index_pk        = std::make_pair(INDEX_KEY.first, index_key);
    std::vector<AttributeValuePair> index_values = {
        {INDEX_ATTRIBUTES[0].first, indexer.peer_id},
        {INDEX_ATTRIBUTES[1].first, uuid}
    };

    res = insertOrUpdate(db, 
                         INDEX_NAME, 
                         index_pk, 
                         index_values, 
                         index_condition);
    if (res == EXIT_FAILURE)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int Database::dropIndex(const uint64_t uuid, const SourceInfo& indexer) {
    std::string        index_key = indexKey(indexer, uuid);
    AttributeValuePair index_pk  = std::make_pair(INDEX_KEY.first, index_key);
    auto err_val = doDelete(db, INDEX_NAME, index_pk);
    if (err_val)
        return reportError(err_val.value());
    return EXIT_SUCCESS;
}

int Database::grabSources(const uint64_t&          uuid,
                          std::vector<SourceInfo>& dest) {
    std::vector<Row> peers;

    //select on file uuid
    std::string select_constraint = INDEX_ATTRIBUTES[1].first + "=" + std::to_string(uuid);

    auto err_val = doSelect(db, 
                            INDEX_NAME, 
                            {INDEX_ATTRIBUTES[0].first}, 
                            {select_constraint},
                            &peers);
    if (err_val)
        return reportError(err_val.value());
    else if (peers.empty())
        return reportError("No peers are indexing this file.");
    
    std::vector<std::string> to_select = {
        PEER_KEY.first,
        PEER_ATTRIBUTES[0].first,
        PEER_ATTRIBUTES[1].first
    };

    dest.clear();
    for (Row& r : peers) {
        std::vector<Row> peer_row;
        SourceInfo s;
        select_constraint = PEER_KEY.first + "=" + r[0]; //id=[id selected above]
        err_val = doSelect(db,
                           PEER_NAME,
                           to_select,
                           {select_constraint},
                           &peer_row);
        if (err_val)
            return reportError(err_val.value());

        if (peer_row[0].size() != 3) {
            return reportError("Missing data for peer.");
        }

        s.peer_id = std::stoull(peer_row[0][0]);
        s.ip_addr = peer_row[0][1];
        s.port    = std::stoul(peer_row[0][2]);
        dest.push_back(s);
    }

    return EXIT_SUCCESS;
}

int Database::updateClient(const SourceInfo& indexer) {
    std::string        peer_condition = PEER_KEY.first + "=" + std::to_string(indexer.peer_id);
    AttributeValuePair peer_pk        = std::make_pair(PEER_KEY.first, indexer.peer_id);
    std::vector<AttributeValuePair> peer_values = {
        {PEER_ATTRIBUTES[0].first, indexer.ip_addr},
        {PEER_ATTRIBUTES[1].first, indexer.port}
    };

    int res = insertOrUpdate(db,
                             PEER_NAME,
                             peer_pk,
                             peer_values,
                             peer_condition);
    return res;
}

}
