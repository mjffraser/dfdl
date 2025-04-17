#pragma once

#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "server/internal/internal/databaseTypes.hpp"

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * createTable
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Creates a table in the database.
 *
 * Takes:
 * -> db:
 *    The SQLite database to modify.
 * -> name:
 *    The name of the table to create.
 * -> primary_key:
 *    The primary key for the table.
 * -> foreign_keys:
 *    A list of foreign keys. These must already exist elsewhere at the time of
 *    creation.
 * -> attributes:
 *    Remaining non-primary & non-foreign table attributes.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> createTable(sqlite3*                      db, 
                                       const std::string             name, 
                                       const TableKey                primary_key, 
                                       const std::vector<ForeignKey> foreign_keys, 
                                       const std::vector<TableKey>   attributes);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * dropTable
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Drops a table from the database.
 *
 * Takes:
 * -> db:
 *    The SQLite database to modify.
 * -> name:
 *    The table name to drop.

 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> dropTable(sqlite3* db, const std::string& name);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doSelect
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Selects rows from a table based on a set of conditions.
 *
 * Takes:
 * -> db:
 *    The SQLite database.
 * -> table_name:
 *    The table to select from.
 * -> attributes:
 *    The attributes to select. A single * can be used to get the entire row.
 * -> conditions:
 *    Conditions to select on. Can be empty.
 * -> dest:
 *    Where to put the selected rows.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doSelect(sqlite3*                       db,
                                    const std::string              table_name,
                                    const std::vector<std::string> attributes,
                                    const std::vector<std::string> conditions,
                                          std::vector<Row>*        dest);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doInsert
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Insert a row into a table. Expects values for every attribute in the
 *    table.
 *
 * Takes:
 * -> db:
 *    The SQLite database.
 * -> table_name:
 *    The table to insert into.
 * -> values:
 *    List of attribute names and values to go with.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doInsert(sqlite3*                              db,
                                    const std::string                     table_name,
                                    const std::vector<AttributeValuePair> values);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doUpdate
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Update a row in a table. Selects the row to update via primary key. 
 *
 * Takes:
 * -> db:
 *    The SQLite database.
 * -> table_name:
 *    The table to update.
 * -> pk_pair:
 *    The primary key value of the row to update.
 * -> values:
 *    The attribute values to update.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doUpdate(sqlite3*                              db,
                                    const std::string                     table_name,
                                    const AttributeValuePair              pk_pair,
                                    const std::vector<AttributeValuePair> values);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doDelete
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Delete a row in a table.
 *
 * Takes:
 * -> db:
 *    The SQLite database.
 * -> table_name:
 *    The name of the table to delete from.
 * -> pk_pair:
 *    The primary key of the row to delete.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doDelete(sqlite3*                 db,
                                    const std::string        table_name,
                                    const AttributeValuePair pk_pair);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doAttach
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Attach a SQLite database file to the open db. This should only be used for
 *    merging database rows.
 *
 * Takes:
 * -> db:
 *    The open SQLite database to attach to.
 * -> to_attach:
 *    The path to the database to attach.
 * -> attach_as:
 *    The key to access the attached database with (XYZ.PEERS or XYZ.FILES).
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doAttach(sqlite3*           db,
                                    const std::string& to_attach,
                                    const std::string& attach_as);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doInsertOrIgnore
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Does an INSERT OR IGNORE operation from table_name in the attached
 *    database into the open database. This should only be used for merging
 *    database rows.
 *
 * Takes:
 * -> db:
 *    The open SQLite database.
 * -> attached_as:
 *    The key to access the attached database with (XYZ.PEERS, XYZ.FILES).
 * -> table_name:
 *    The table present in both databases to copy.
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doInsertOrIgnore(sqlite3*           db,
                                            const std::string& attached_as,
                                            const std::string& table_name);

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * doDetach
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Detaches the attached database. This should only be used for merging
 *    database rows.
 *
 * Takes:
 * -> db:
 *    The open SQLite database with an attached database.
 * -> attached_as:
 *    The key to access the attached database with (XYZ.PEERS, XYZ.FILES).
 *
 * Returns:
 * -> On success:
 *    std::nullopt
 * -> On failure:
 *    Error message.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
std::optional<std::string> doDetach(sqlite3*           db,
                                    const std::string& attached_as);

} //dfd
