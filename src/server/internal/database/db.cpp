#include "server/internal/database/db.hpp"
#include "server/internal/database/internal/types.hpp"
#include "server/internal/database/internal/queries.hpp"
#include "server/internal/database/internal/tableInfo.hpp"
#include "sourceInfo.hpp"
#include <iostream>
#include <string>
#include <utility>

namespace dfd {

std::string Database::sqliteError() {
    return sqlite3_errmsg(db);
}

int Database::setupDatabase() {
    std::vector<ForeignKey> no_fk; //for tables without foreign keys

    //create tables
    if (SQLITE_OK != createTable(db, 
                                 PEER_NAME, 
                                 PEER_KEY,
                                 no_fk,
                                 PEER_ATTRIBUTES))
        return EXIT_FAILURE;

    if (SQLITE_OK != createTable(db,
                                 FILE_NAME,
                                 FILE_KEY,
                                 no_fk,
                                 FILE_ATTRIBUTES))
        return EXIT_FAILURE;

    if (SQLITE_OK != createTable(db,
                                 INDEX_NAME,
                                 INDEX_KEY,
                                 INDEX_FK,
                                 INDEX_ATTRIBUTES))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * insertOrUpdate
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Checks SQLite database for an entry. If it exists, updates the row with
 *    the provided values. If it doesn't, inserts a row into the database.
 *
 * Takes:
 * -> db:
 *    The SQLite database to operate on.
 * -> table_name:
 *    The table name to select, and insert or update with.
 * -> pk_pair:
 *    The primary key of the row to select.
 * -> values:
 *    The value pairs in the form <"key_name", value> to insert/update. Is
 *    modified if an insertion occurs to add primary key value pair in.
 * -> pk_condition:
 *    The condition to select the row with. Ex. "id=16274526523232"
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/
int insertOrUpdate(sqlite3*                               db, 
                   const std::string&                     table_name, 
                   const AttributeValuePair&              pk_pair, 
                         std::vector<AttributeValuePair>& values,
                   const std::string&                     pk_condition) {
    std::vector<Row> temp; 
    if (SQLITE_OK != doSelect(db, table_name, {pk_pair.first}, {pk_condition}, &temp))
        return EXIT_FAILURE;
    
    if (temp.size() == 0) {
        //no entry, so do insert
        values.push_back(pk_pair); //add primary key value into values
        if (SQLITE_OK != doInsert(db, table_name, values))
            return EXIT_FAILURE;
    } else {
        //otherwise, we just update stuff
        if (SQLITE_OK != doUpdate(db, table_name, pk_pair, values))
            return EXIT_FAILURE;
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

    if (EXIT_SUCCESS != insertOrUpdate(db, 
                                       PEER_NAME, 
                                       peer_pk, 
                                       peer_values, 
                                       peer_condition))
        return EXIT_FAILURE; //couldn't add indexer into table

    //now we need to check if the file to index already exists
    std::string        file_condition = FILE_KEY.first + "=" + std::to_string(uuid);
    AttributeValuePair file_pk        = std::make_pair(FILE_KEY.first, uuid);
    std::vector<AttributeValuePair> file_values = {
        {FILE_ATTRIBUTES[0].first, f_size}
    };

    if (EXIT_SUCCESS != insertOrUpdate(db, 
                                       FILE_NAME, 
                                       file_pk, 
                                       file_values, 
                                       file_condition))
        return EXIT_FAILURE; //couldn't add file to table

    //finally, associate the indexer with the file
    std::string        index_key       = indexKey(indexer, uuid);
    std::string        index_condition = INDEX_KEY.first + "=" + index_key;
    AttributeValuePair index_pk        = std::make_pair(INDEX_KEY.first, index_key);
    std::vector<AttributeValuePair> index_values = {
        {INDEX_ATTRIBUTES[0].first, indexer.peer_id},
        {INDEX_ATTRIBUTES[1].first, uuid}
    };

    if (EXIT_SUCCESS != insertOrUpdate(db, 
                                       INDEX_NAME, 
                                       index_pk, 
                                       index_values, 
                                       index_condition))
        return EXIT_FAILURE; //couldn't link file to indexer

    return EXIT_SUCCESS;
}

int Database::dropIndex(const uint64_t uuid, const SourceInfo& indexer) {
    std::string        index_key = indexKey(indexer, uuid);
    AttributeValuePair index_pk  = std::make_pair(INDEX_KEY.first, index_key);
    if (SQLITE_OK != doDelete(db, INDEX_NAME, index_pk))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int Database::grabSources(const uint64_t&          uuid,
                          std::vector<SourceInfo>& dest,
                          uint64_t                 f_size) {
    std::vector<Row> peers;

    //select on file uuid
    std::string select_constraint = INDEX_ATTRIBUTES[1].first + "=" + std::to_string(uuid);

    if (SQLITE_OK != doSelect(db, 
                              INDEX_NAME, 
                              {INDEX_ATTRIBUTES[0].first}, 
                              {select_constraint},
                              &peers))
        return EXIT_FAILURE;
    
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
        if (SQLITE_OK != doSelect(db,
                                  PEER_NAME,
                                  to_select,
                                  {select_constraint},
                                  &peer_row))
            return EXIT_FAILURE;

        if (peer_row[0].size() != 3) {
            return EXIT_FAILURE;
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

    if (EXIT_SUCCESS != insertOrUpdate(db,
                                       PEER_NAME,
                                       peer_pk,
                                       peer_values,
                                       peer_condition))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

}
