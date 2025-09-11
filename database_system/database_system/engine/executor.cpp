#include "executor.hpp"
#include <iostream>
#include <sstream>

// ==========================
// Execute main
// ==========================
bool Executor::Execute(const std::string& sql) {
    std::istringstream iss(sql);
    std::string command;
    iss >> command;

    if (command == "CREATE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // TABLE tableName
        // TODO: parse columns
        std::vector<Column> columns;
        return ExecuteCreateTable(tableName, columns, tableName + ".tbl");
    }
    else if (command == "INSERT") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // INTO tableName
        iss >> tmp;              // VALUES
        char ch;
        iss >> ch; // skip '('
        std::vector<std::string> values;
        std::string val;
        while (iss >> val) {
            if (val.back() == ',' || val.back() == ')') {
                if (val.back() == ',' || val.back() == ')')
                    val.pop_back();
                values.push_back(val);
                if (val.back() == ')') break;
            }
            else {
                values.push_back(val);
            }
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
// Wrappers
// ==========================
bool Executor::CreateTable(const std::string& tableName,
    const std::vector<Column>& columns,
    const std::string& fileName) {
    return ExecuteCreateTable(tableName, columns, fileName);
}

bool Executor::Insert(const std::string& tableName,
    const std::vector<std::string>& values) {
    return ExecuteInsert(tableName, values);
}

bool Executor::Select(const std::string& tableName,
    const std::vector<std::string>& columns,
    const std::string& whereCol,
    const std::string& whereVal) {
    return ExecuteSelect(tableName, columns, whereCol, whereVal);
}

bool Executor::Delete(const std::string& tableName,
    const std::string& whereCol,
    const std::string& whereVal) {
    return ExecuteDelete(tableName, whereCol, whereVal);
}

bool Executor::DropTable(const std::string& tableName) {
    return ExecuteDropTable(tableName);
}

void Executor::ShowTables() {
    ExecuteShowTables();
}

// ==========================
// Thực thi CREATE TABLE
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
// Thực thi INSERT
// ==========================
bool Executor::ExecuteInsert(const std::string& tableName,
    const std::vector<std::string>& values) {
    std::cout << "Inserting into " << tableName << " values: ";
    for (auto& v : values) std::cout << v << " ";
    std::cout << "\n";
    // TODO: gọi RecordManager.Insert
    return true;
}

// ==========================
// Thực thi SELECT
// ==========================
bool Executor::ExecuteSelect(const std::string& tableName,
    const std::vector<std::string>& columns,
    const std::string& whereCol,
    const std::string& whereVal) {
    std::cout << "Selecting from " << tableName << "\n";
    if (!whereCol.empty()) {
        std::cout << "Where " << whereCol << " = " << whereVal << "\n";
    }
    // TODO: gọi RecordManager.Select
    return true;
}

// ==========================
// Thực thi DELETE
// ==========================
bool Executor::ExecuteDelete(const std::string& tableName,
    const std::string& whereCol,
    const std::string& whereVal) {
    std::cout << "Deleting from " << tableName << "\n";
    if (!whereCol.empty()) {
        std::cout << "Where " << whereCol << " = " << whereVal << "\n";
    }
    // TODO: gọi RecordManager.Delete
    return true;
}

// ==========================
// Thực thi DROP TABLE
// ==========================
bool Executor::ExecuteDropTable(const std::string& tableName) {
    if (catalogManager.DropTable(catalog, tableName)) {
        std::cout << "Table '" << tableName << "' dropped.\n";
        return true;
    }
    return false;
}

// ==========================
// Thực thi SHOW TABLES
// ==========================
void Executor::ExecuteShowTables() {
    std::cout << "Tables in catalog:\n";
    for (const auto& tableName : catalog.ListTables()) {
        std::cout << " - " << tableName << "\n";
    }
}
