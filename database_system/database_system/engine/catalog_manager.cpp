#include "catalog_manager.hpp"
#include <fstream>
#include <iostream>

// Constructor
CatalogManager::CatalogManager(const std::string& file_path)
    : file_path(file_path) {
}

// Load catalog từ file
bool CatalogManager::LoadCatalog(Catalog& catalog) {
    std::ifstream in(file_path);
    if (!in.is_open()) {
        std::cerr << "⚠️ Warning: catalog file not found, starting with empty catalog.\n";
        return false;
    }

    std::string tableName, fileName;
    int colCount;

    while (in >> tableName >> fileName >> colCount) {
        std::vector<Column> cols;
        for (int i = 0; i < colCount; i++) {
            std::string colName, typeStr;
            int length, pkFlag, notNullFlag;
            in >> colName >> typeStr >> length >> pkFlag >> notNullFlag;

            ColumnType type;
            if (typeStr == "INT") type = ColumnType::INT;
            else if (typeStr == "FLOAT") type = ColumnType::FLOAT;
            else type = ColumnType::VARCHAR;

            cols.emplace_back(colName, type, length,
                pkFlag == 1, notNullFlag == 1);
        }
        Schema schema(cols);
        catalog.AddTable(tableName, schema, fileName);
    }
    return true;
}

// Save catalog xuống file
bool CatalogManager::SaveCatalog(const Catalog& catalog) {
    std::ofstream out(file_path, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "❌ Error: cannot open catalog file for writing.\n";
        return false;
    }

    auto tables = catalog.ListTables();
    for (const auto& t : tables) {
        TableInfo* info = const_cast<Catalog&>(catalog).GetTable(t);
        if (!info) continue;

        out << info->getName() << " " << info->getFileName() << " "
            << info->getSchema().GetColumnCount() << "\n";

        for (const auto& col : info->getSchema().GetColumns()) {
            std::string typeStr;
            switch (col.type) {
            case ColumnType::INT: typeStr = "INT"; break;
            case ColumnType::FLOAT: typeStr = "FLOAT"; break;
            case ColumnType::VARCHAR: typeStr = "VARCHAR"; break;
            }
            out << col.name << " " << typeStr << " "
                << col.length << " "
                << (col.isPrimaryKey ? 1 : 0) << " "
                << (col.isNotNull ? 1 : 0) << "\n";
        }
    }
    return true;
}

// Thêm bảng
bool CatalogManager::CreateTable(Catalog& catalog,
    const std::string& tableName,
    const Schema& schema,
    const std::string& fileName) {
    if (catalog.GetTable(tableName)) {
        std::cerr << "❌ Error: table " << tableName << " already exists.\n";
        return false;
    }
    catalog.AddTable(tableName, schema, fileName);
    return SaveCatalog(catalog);
}

// Xóa bảng
bool CatalogManager::DropTable(Catalog& catalog, const std::string& tableName) {
    if (!catalog.RemoveTable(tableName)) {
        std::cerr << "❌ Error: table " << tableName << " not found.\n";
        return false;
    }
    return SaveCatalog(catalog);
}
