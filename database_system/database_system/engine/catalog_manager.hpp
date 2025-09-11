#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

// ==========================
// ColumnType định nghĩa kiểu dữ liệu cột
// ==========================
enum class ColumnType {
    INT,
    FLOAT,
    VARCHAR
};

// ==========================
// Column định nghĩa 1 cột
// ==========================
struct Column {
    std::string name;
    ColumnType type;
    int length; // chỉ dùng cho VARCHAR

    // Constraint cơ bản
    bool isPrimaryKey;
    bool isNotNull;

    Column(const std::string& n,
        ColumnType t,
        int len = 0,
        bool pk = false,
        bool notNull = false)
        : name(n),
        type(t),
        length(len),
        isPrimaryKey(pk),
        isNotNull(notNull) {
    }
};

// ==========================
// Schema định nghĩa 1 bảng
// ==========================
class Schema {
public:
    Schema() = default;
    explicit Schema(const std::vector<Column>& cols) : columns(cols) {}

    const std::vector<Column>& GetColumns() const { return columns; }
    int GetColumnCount() const { return (int)columns.size(); }

private:
    std::vector<Column> columns;
};

// ==========================
// TableInfo chứa metadata bảng
// ==========================
struct TableInfo {
    std::string name;
    Schema schema;
    std::string file_name;

    TableInfo(const std::string& n, const Schema& s, const std::string& f)
        : name(n), schema(s), file_name(f) {
    }

    const std::string& getName() const { return name; }
    const Schema& getSchema() const { return schema; }
    const std::string& getFileName() const { return file_name; }
};

// ==========================
// Catalog quản lý metadata
// ==========================
class Catalog {
public:
    void AddTable(const std::string& name,
        const Schema& schema,
        const std::string& fileName) {
        tables[name] = std::make_unique<TableInfo>(name, schema, fileName);
    }

    TableInfo* GetTable(const std::string& name) {
        auto it = tables.find(name);
        if (it != tables.end()) return it->second.get();
        return nullptr;
    }

    std::vector<std::string> ListTables() const {
        std::vector<std::string> result;
        for (const auto& pair : tables) {
            result.push_back(pair.first);
        }
        return result;
    }

    bool RemoveTable(const std::string& name) {
        return tables.erase(name) > 0;
    }

private:
    std::map<std::string, std::unique_ptr<TableInfo>> tables;
};

// ==========================
// CatalogManager quản lý file catalog
// ==========================
class CatalogManager {
public:
    explicit CatalogManager(const std::string& file_path);

    bool LoadCatalog(Catalog& catalog);
    bool SaveCatalog(const Catalog& catalog);

    bool CreateTable(Catalog& catalog,
        const std::string& tableName,
        const Schema& schema,
        const std::string& fileName);

    bool DropTable(Catalog& catalog, const std::string& tableName);

private:
    std::string file_path;
};
