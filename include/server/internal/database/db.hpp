#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>

#include <sqlite3.h>

#include "server/internal/database/internal/types.hpp"

namespace dfd {

struct SourceInfo; //definition in src

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Database
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A class object to manage the database instance for a given server. 
 *    Contains functionality to index files, drop indexes, get a list of the
 *    peers indexing a paticular file, serialization of a database for network
 *    transfer, and the ability to merge a serialized database into the open
 *    one.
 *
 * Member Variables:
 * -> db:
 *    The SQLite database instance.
 *
 * Constructor:
 * -> Takes:
 *    -> db_path:
 *       A path to the database file to either open or create.
 * -> Throws:
 *    -> SQLError:
 *       Any error that occurs when interacting with an SQLite database.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
class Database {
private:
    sqlite3*    db;
    std::string err_msg = ""; //set on any error
        
    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * setupDatabase
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Function called by constructor if an existing database file
     *    wasn't found.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS
     * -> On failure:
     *    EXIT_FAILURE
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int setupDatabase();
        
    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * reportError
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Called internally when an error occurs to populate an error message
     *    for sqliteError() to return.
     *
     * Takes:
     * -> The error message.
     *
     * Returns:
     * -> On success:
     *    EXIT_FAILURE
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int reportError(const std::string& err_msg);


    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * insertOrUpdate
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Checks SQLite database for an entry. If it exists, updates the row 
     *    with the provided values. If it doesn't, inserts a row into the
     *    database.
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
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int insertOrUpdate(sqlite3*                               db, 
                       const std::string&                     table_name, 
                       const AttributeValuePair&              pk_pair, 
                             std::vector<AttributeValuePair>& values,
                       const std::string&                     pk_condition);

public:
    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * sqliteError
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Reports the most recent SQLite database error. Should be called after
     *    one of the below functions reports EXIT_FAILURE.
     *
     * Returns:
     * -> On success:
     *    The error message as a std::string
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    std::string sqliteError();

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * backupDatabase
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Copies from the open database into a SQLite database file at path.
     *
     * Takes:
     * -> path:
     *    A full path for where to write the copy to, relative or absolute.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS
     * -> On failure:
     *    EXIT_FAILURE
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int backupDatabase(const std::string& path);

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * mergeDatabases
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Merges all the rows from a SQLite database at path into the
     *    currently open and managed database. Any rows that already exist in
     *    the database being copied in to will be skipped based on duplicate
     *    primary keys.
     *
     * Takes:
     * -> path:
     *    A full path for the SQLite database to copy from, relative or
     *    absolute.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int mergeDatabases(const std::string& path);

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * indexFile
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Takes a files uuid, and the needed info about the indexer and adds it
     *    to the database.
     * 
     * Takes:
     * -> uuid:
     *    A unique identifier for a file to index. Should be obtained from
     *    fileParsing.
     * -> indexer:
     *    SourceInfo struct about the file sources info.
     * -> f_size:
     *    The size of the file, in bytes.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS
     * -> On failure:
     *    EXIT_FAILURE
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int indexFile(const uint64_t     uuid, 
                  const SourceInfo&  indexer,
                  const uint64_t     f_size);

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * dropIndex
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Removes a database entry for a certain file for a certain user. 
     * 
     * Takes:
     * -> uuid:
     *    A unique identifier for a file to index. Should be obtained from
     *    fileParsing.
     * -> indexer:
     *    SourceInfo struct about the file sources info.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS
     * -> On failure:
     *    EXIT_FAILURE
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int dropIndex(const uint64_t uuid, const SourceInfo& indexer);

    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * grabSources
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Given a file uuid, returns a list of SourceInfo about various users
     *    that have a file indexed.
     *
     * Takes:
     * -> uuid:
     *    A unique identifier for a file to index. Should be obtained from
     *    fileParsing.
     * -> dest:
     *    A vector to put the result into. If no indexers are found the vector
     *    is CLEARED.
     * -> f_size:
     *    The size of the file being downloaded, in bytes.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS, even if no indexed entries found.
     * -> On failure:
     *    EXIT_FAILURE, on a critical error.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int grabSources(const uint64_t&          uuid,
                    std::vector<SourceInfo>& dest);


    /*
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * updateClient
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * Description:
     * -> Updates a clients source info within their database row. If no row
     *    exists, one is inserted.
     *
     * Takes:
     * -> indexer:
     *    A SourceInfo object with the up-to-date information.
     *
     * Returns:
     * -> On success:
     *    EXIT_SUCCESS, even if no indexed entries found.
     * -> On failure:
     *    EXIT_FAILURE, on a critical error.
     * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     */
    int updateClient(const SourceInfo& indexer);
    
    //CONSTRUCTOR
    Database(const std::string& db_path) {
        std::ifstream db_file(db_path);
        bool existed = db_file.good();
        int res = sqlite3_open(db_path.c_str(), &db);

        if (!existed) {
            if (setupDatabase() != EXIT_SUCCESS)
                throw std::runtime_error(sqliteError());
        }

        if (sqlite3_exec(db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqliteError());
        }
    }

    //DESTRUCTOR
    ~Database() {
        sqlite3_close(db);
    }
};

} //dfd 
