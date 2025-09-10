#include "executor.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

// ==========================
// Execute main
// ==========================
bool Executor::Execute(const std::string& sql) {
    std::istringstream iss(sql);
    std::string command;
    iss >> command;

    if (command == "CREATE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // "TABLE tableName"
        std::vector<Column> columns;
        std::string colName, colType;
        int length;

        // Đọc cột: giả sử cú pháp "CREATE TABLE name (id INT, name VARCHAR 50, gpa FLOAT);"
        char ch;
        iss >> ch; // skip '('
        while (iss >> colName >> colType) {
            length = 0;
            if (colType == "VARCHAR") {
                iss >> length;
            }
            columns.emplace_back(colName,
                colType == "INT" ? ColumnType::INT :
                colType == "FLOAT" ? ColumnType::FLOAT : ColumnType::VARCHAR,
                length);

            iss >> ch; // skip ',' hoặc ')'
            if (ch == ')') break;
        }

        return ExecuteCreateTable(tableName, columns, tableName + ".tbl");
    }
    else if (command == "DROP") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // "TABLE tableName"
        return ExecuteDropTable(tableName);
    }
    else if (command == "SHOW") {
        std::string tmp;
        iss >> tmp; // "TABLES"
        ExecuteShowTables();
        return true;
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        return false;
    }
}

// ==========================
// CREATE TABLE
// ==========================
bool Executor::ExecuteCreateTable(const std::string& tableName, const std::vector<Column>& columns, const std::string& fileName) {
    Schema schema(columns);
    if (catalogManager.CreateTable(catalog, tableName, schema, fileName)) {
        std::cout << "Table '" << tableName << "' created successfully.\n";
        return true;
    }
    return false;
}

// ==========================
// DROP TABLE
// ==========================
bool Executor::ExecuteDropTable(const std::string& tableName) {
    if (catalogManager.DropTable(catalog, tableName)) {
        std::cout << "Table '" << tableName << "' dropped successfully.\n";
        return true;
    }
    return false;
}

// ==========================
// SHOW TABLES
// ==========================
void Executor::ExecuteShowTables() {
    std::cout << "Tables in catalog:\n";
    for (const auto& tableName : catalog.ListTables()) {
        std::cout << " - " << tableName << "\n";
    }
}
