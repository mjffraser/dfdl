#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

#include <sqlite3.h>

namespace dfd {

struct SourceInfo; //definition in src

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Database
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> A class object to manage the database instance for a given server. 
 *    Contains functionality to index files, drop indexes, and get a list of the
 *    peers indexing a paticular file.
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
    sqlite3* db;
    
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

public:
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
    int indexFile(const std::string& uuid, 
                  const SourceInfo&  indexer,
                  const uint64_t&    f_size);

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
    int dropIndex(const std::string& uuid, const SourceInfo& indexer);

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
    int grabSources(const std::string&       uuid,
                    std::vector<SourceInfo>& dest,
                    uint64_t&                f_size);
    
    //CONSTRUCTOR
    Database(const std::string& db_path) {
        std::ifstream db_file(db_path);
        bool existed = db_file.good();
        int res = sqlite3_open(db_path.c_str(), &db);

        if (!existed) {
            if (setupDatabase() != EXIT_SUCCESS)
                ; //err 
        }

        if (sqlite3_exec(db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr) != SQLITE_OK) {
            ; //err
        }
    }  
};

} //dfd 
