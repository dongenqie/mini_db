#include "executor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ==========================
// Parse & Dispatch SQL
// ==========================
bool Executor::Execute(const std::string& sql) {
    std::istringstream iss(sql);
    std::string command;
    iss >> command;

    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "CREATE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // TABLE tableName

        // Parse danh sách cột: (colName type [len], ...)
        char ch;
        iss >> ch; // (
        std::vector<Column> columns;
        while (iss.peek() != ')' && iss.good()) {
            std::string colName, typeStr;
            int len = 0;
            iss >> colName >> typeStr;

            ColumnType type;
            if (typeStr == "INT") type = ColumnType::INT;
            else if (typeStr == "FLOAT") type = ColumnType::FLOAT;
            else {
                type = ColumnType::VARCHAR;
                iss >> len; // lấy độ dài
            }

            columns.emplace_back(colName, type, len);

            if (iss.peek() == ',') iss.ignore();
        }
        iss >> ch; // )

        return ExecuteCreateTable(tableName, columns, tableName + ".tbl");
    }
    else if (command == "INSERT") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // INTO tableName
        iss >> tmp;              // VALUES
        char ch;
        iss >> ch; // (
        std::vector<std::string> values;
        std::string val;
        while (iss >> val) {
            if (val.back() == ',' || val.back() == ')') {
                if (val.back() == ',' || val.back() == ')')
                    val.pop_back();
                values.push_back(val);
            }
            else {
                values.push_back(val);
            }
            if (iss.peek() == ')') { iss >> ch; break; }
        }
        return ExecuteInsert(tableName, values);
    }
    else if (command == "SELECT") {
        std::vector<std::string> cols;
        std::string col;
        while (iss >> col && col != "FROM") {
            if (col.back() == ',') col.pop_back();
            cols.push_back(col);
        }
        std::string tableName;
        iss >> tableName;
        std::string whereCol, whereVal, tmp;
        if (iss >> tmp && tmp == "WHERE") {
            iss >> whereCol >> tmp >> whereVal; // col = value
        }
        return ExecuteSelect(tableName, cols, whereCol, whereVal);
    }
    else if (command == "DELETE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // FROM tableName
        std::string whereCol, whereVal;
        if (iss >> tmp && tmp == "WHERE") {
            iss >> whereCol >> tmp >> whereVal; // col = value
        }
        return ExecuteDelete(tableName, whereCol, whereVal);
    }
    else if (command == "DROP") {
        std::string tmp, tableName;
        iss >> tmp >> tableName;
        return ExecuteDropTable(tableName);
    }
    else if (command == "SHOW") {
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
bool Executor::ExecuteCreateTable(const std::string& tableName,
    const std::vector<Column>& columns,
    const std::string& fileName) {
    Schema schema(columns);
    if (catalogManager.CreateTable(catalog, tableName, schema, fileName)) {
        std::cout << "Table '" << tableName << "' created.\n";
        return true;
    }
    return false;
}

// ==========================
// INSERT
// ==========================
bool Executor::ExecuteInsert(const std::string& tableName,
    const std::vector<std::string>& values) {
    TableInfo* table = catalog.GetTable(tableName);
    if (!table) {
        std::cerr << "Error: table " << tableName << " not found.\n";
        return false;
    }

    // TODO: gọi RecordManager.Insert
    std::cout << "[RecordManager] Insert into " << tableName << " values: ";
    for (auto& v : values) std::cout << v << " ";
    std::cout << "\n";

    return true;
}

// ==========================
// SELECT
// ==========================
bool Executor::ExecuteSelect(const std::string& tableName,
    const std::vector<std::string>& columns,
    const std::string& whereCol,
    const std::string& whereVal) {
    TableInfo* table = catalog.GetTable(tableName);
    if (!table) {
        std::cerr << "Error: table " << tableName << " not found.\n";
        return false;
    }

    std::cout << "Executing SELECT on " << tableName << "\n";

    // Toán tử 1: SeqScan
    std::cout << "[SeqScan] Table: " << tableName << "\n";

    // Toán tử 2: Filter
    if (!whereCol.empty()) {
        std::cout << "[Filter] Condition: " << whereCol << " = " << whereVal << "\n";
    }

    // Toán tử 3: Project
    if (!columns.empty() && !(columns.size() == 1 && columns[0] == "*")) {
        std::cout << "[Project] Columns: ";
        for (auto& c : columns) std::cout << c << " ";
        std::cout << "\n";
    }
    else {
        std::cout << "[Project] All columns\n";
    }

    // TODO: gọi RecordManager.Select
    std::cout << "[RecordManager] Scan finished. (rows printed here)\n";

    return true;
}

// ==========================
// DELETE
// ==========================
bool Executor::ExecuteDelete(const std::string& tableName,
    const std::string& whereCol,
    const std::string& whereVal) {
    TableInfo* table = catalog.GetTable(tableName);
    if (!table) {
        std::cerr << "Error: table " << tableName << " not found.\n";
        return false;
    }

    std::cout << "Executing DELETE on " << tableName << "\n";
    if (!whereCol.empty()) {
        std::cout << "[Filter] Condition: " << whereCol << " = " << whereVal << "\n";
    }

    // TODO: gọi RecordManager.Delete
    std::cout << "[RecordManager] Delete finished.\n";

    return true;
}

// ==========================
// DROP TABLE
// ==========================
bool Executor::ExecuteDropTable(const std::string& tableName) {
    if (catalogManager.DropTable(catalog, tableName)) {
        std::cout << "Table '" << tableName << "' dropped.\n";
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
