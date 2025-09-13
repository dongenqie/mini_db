// =============================================
// engine/catalog_manager.cpp （替换/补丁）
// =============================================
#include "catalog_manager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

using std::filesystem::path;
using std::filesystem::create_directories;

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string to_upper(const std::string& s) {
    std::string r = s; for (auto& c : r) c = (char)std::toupper((unsigned char)c); return r;
}

bool CatalogManager::PersistCatalog(const Catalog& cat, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;

    // 逐表写： name|file|first|last|col_count|col1|col2|...
    for (const auto& tname : cat.ListTables()) {
        const TableInfo* t = cat.GetTable(tname);
        if (!t) continue;

        std::ostringstream line;
        line << t->name << "|"
            << t->file_name << "|"
            << t->first_pid << "|"
            << t->last_pid << "|";

        const auto& cols = t->getSchema().GetColumns();
        line << cols.size();
        for (auto& c : cols) {
            // 举例：id:INT 或 name:VARCHAR(64)
            line << "|" << c.name << ":"
                << (c.type == ColumnType::INT ? "INT" :
                    c.type == ColumnType::FLOAT ? "FLOAT" :
                    "VARCHAR(" + std::to_string(c.length) + ")");
        }
        out << line.str() << "\n";
    }
    out.flush();
    return (bool)out;
}

CatalogManager::CatalogManager(const std::string& file_path)
    : file_path(file_path) {}

// Load catalog từ file
bool CatalogManager::LoadCatalog(Catalog& catalog) {
    std::ifstream ifs(file_path);
    if (!ifs) return true; // 没文件视为空目录
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string name, file, tok;

        std::getline(ss, name, '|');
        std::getline(ss, file, '|');
        std::getline(ss, tok, '|'); uint32_t first_pid = tok.empty() ? 0 : static_cast<uint32_t>(std::stoul(tok));
        std::getline(ss, tok, '|'); uint32_t last_pid = tok.empty() ? 0 : static_cast<uint32_t>(std::stoul(tok));
        std::getline(ss, tok, '|'); int col_cnt = tok.empty() ? 0 : std::stoi(tok);

        std::vector<Column> cols;
        cols.reserve(col_cnt);
        for (int i = 0; i < col_cnt; ++i) {
            std::string coldef;
            if (!std::getline(ss, coldef, '|')) break;
            // 解析：name:TYPE 或 name:VARCHAR(64)
            auto pos = coldef.find(':');
            if (pos == std::string::npos) continue;
            std::string cname = coldef.substr(0, pos);
            std::string ctype = coldef.substr(pos + 1);
            ColumnType ct = ColumnType::VARCHAR;
            int clen = 0;
            if (ctype == "INT") ct = ColumnType::INT;
            else if (ctype == "FLOAT") ct = ColumnType::FLOAT;
            else {
                // VARCHAR(n)
                if (ctype.rfind("VARCHAR", 0) == 0) {
                    auto l = ctype.find('('), r = ctype.find(')');
                    if (l != std::string::npos && r != std::string::npos && r > l + 1) {
                        clen = std::stoi(ctype.substr(l + 1, r - l - 1));
                    }
                    ct = ColumnType::VARCHAR;
                }
            }
            cols.emplace_back(cname, ct, clen, false, false);
        }
        Schema schema(cols);
        catalog.AddTable(name, schema, file, first_pid, last_pid);
    }
    return true;
}

// Save catalog xuống file
bool CatalogManager::SaveCatalog(const Catalog& catalog) {
    std::ofstream ofs(file_path, std::ios::trunc);
    if (!ofs) return false;

    // 不能直接访问 map，给 Catalog 增个导出接口更优；这里简化：ListTables + GetTable
    for (const auto& tname : catalog.ListTables()) {
        const TableInfo* t = catalog.GetTable(tname);
        if (!t) continue;

        ofs << t->name << '|'
            << t->file_name << '|'
            << t->first_pid << '|'
            << t->last_pid << '|';

        const auto& cols = t->getSchema().GetColumns();
        ofs << cols.size();
        for (const auto& c : cols) {
            ofs << '|'
                << c.name << ':';
            switch (c.type) {
            case ColumnType::INT:    ofs << "INT"; break;
            case ColumnType::FLOAT:  ofs << "FLOAT"; break;
            case ColumnType::VARCHAR:ofs << "VARCHAR(" << c.length << ")"; break;
            }
        }
        ofs << "\n";
    }
    return true;
}

bool CatalogManager::CreateTable(Catalog& catalog,
    const std::string& tableName,
    const Schema& schema,
    const std::string& fileName) {
    if (catalog.GetTable(tableName)) return false;
    catalog.AddTable(tableName, schema, fileName, 0, 0);
    return SaveCatalog(catalog);
}

bool CatalogManager::DropTable(Catalog& catalog, const std::string& tableName) {
    if (!catalog.RemoveTable(tableName)) return false;
    return SaveCatalog(catalog);
}

bool CatalogManager::UpdateTablePages(Catalog& catalog,
    const std::string& tableName,
    uint32_t first_pid,
    uint32_t last_pid) {
    TableInfo* t = catalog.GetTable(tableName);
    if (!t) return false;
    t->first_pid = first_pid;
    t->last_pid = last_pid;
    return SaveCatalog(catalog); // 简单点：每次更新都持久化一次
}

