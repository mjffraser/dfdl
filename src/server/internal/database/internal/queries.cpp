#include "server/internal/database/internal/queries.hpp"

namespace dfd {

int createTable(sqlite3*                      db, 
                const std::string             name, 
                const TableKey                primary_key, 
                const std::vector<ForeignKey> foreign_keys, 
                const std::vector<TableKey>   attributes
               ) {
    std::string query;
    query += "CREATE TABLE " + name + "(";

    //primary key
    query += primary_key.first + " " + primary_key.second + " PRIMARY KEY NOT NULL";

    //attributes
    for (auto& entry : attributes) {
        query += ",";
        query += entry.first + " " + entry.second;
    }

    //foreign keys
    for (auto& entry : foreign_keys) {
        query += ",FOREIGN KEY (" + std::get<0>(entry) + ") ";
        query += "REFERENCES " + std::get<1>(entry) + "(" + std::get<2>(entry) + ") ON DELETE RESTRICT";
    }

    query += ");";

    //return either SQLITE_OK or the error code to check
    return sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
}

int dropTable(sqlite3* db, const std::string& name) {
    std::string query = "DROP TABLE " + name + ";";
    return sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
}

int callback(void* data, int columns, char** col_vals, char** col_names) {
    auto* selected_rows = static_cast<std::vector<Row>*>(data);
    try {
        selected_rows->emplace_back(col_vals, col_vals+columns);
    } catch (...) {
        return 1;
    }
    return 0;
}

int doSelect(sqlite3*                       db,
             const std::string              table_name,
             const std::vector<std::string> attributes,
             const std::vector<std::string> conditions,
                   std::vector<Row>*        dest
            ) {
    std::string query = "SELECT ";

    //to select
    for (int i = 0; i < attributes.size(); i++) {
        if (i == 0)
            query += attributes.at(i);
        else
            query += "," + attributes.at(i);
    }

    query += " FROM " + table_name;

    //restrict on conditions
    if (!conditions.empty()) {
        query += " WHERE ";
        for (int i = 0; i < conditions.size(); i++) {
            if (i == 0)
                query += conditions.at(i);
            else
                query += " AND " + conditions.at(i);
        }
    }

    query += ";";

    //puts selected rows into dest via callback
    return sqlite3_exec(db, query.c_str(), callback, dest, nullptr);
}

std::string castVariant(const std::variant<uint64_t, uint32_t, std::string>& val) {
    if (auto v = std::get_if<uint64_t>(&val))
        return std::to_string(*v);
    else if (auto v = std::get_if<uint32_t>(&val))
        return std::to_string(*v);
    else if (auto v = std::get_if<std::string>(&val))
        return "'" + *v + "'";
    else
        return "";
}

int doInsert(sqlite3*                              db,
             const std::string                     table_name,
             const std::vector<AttributeValuePair> values
            ) {
    std::string query = "INSERT INTO " + table_name + "(";

    //build value string at the same time
    std::string value_string = "(";
    for (int i = 0; i < values.size(); i++) {
        if (i != 0) {
            query += ",";
            value_string += ",";
        }

        query += values.at(i).first;
        value_string += castVariant(values.at(i).second);
    }

    //append value string
    query += ") VALUES " + value_string + ");";
    
    return sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
}

int doUpdate(sqlite3*                              db,
             const std::string                     table_name,
             const AttributeValuePair              pk_pair,
             const std::vector<AttributeValuePair> values
            ) {
    std::string query = "UPDATE " + table_name + " SET ";

    //to update
    for (int i = 0; i < values.size(); i++) {
        if (i != 0) query += ",";
        query += values.at(i).first + "=" + castVariant(values.at(i).second);
    }

    //primary key to update on
    query += " WHERE " + pk_pair.first + "=" + castVariant(pk_pair.second);
    query += ";";

    return sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
}

int doDelete(sqlite3*                 db,
             const std::string        table_name,
             const AttributeValuePair pk_pair
            ) {
    std::string query = "DELETE FROM " + table_name + " WHERE ";
    query += pk_pair.first + "=" + castVariant(pk_pair.second);
    query += ";";

    return sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
}

} //dfd
