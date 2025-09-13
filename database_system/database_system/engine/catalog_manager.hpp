// =============================================
// engine/catalog_manager.hpp   （覆盖此文件）
// =============================================
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <filesystem>

// 列类型
enum class ColumnType { INT, FLOAT, VARCHAR };

struct Column {
    std::string name;
    ColumnType  type;
    int         length;     // VARCHAR 长度（INT/FLOAT 可设为 0）
    bool        isPrimaryKey;
    bool        isNotNull;
    Column(const std::string& n, ColumnType t, int len = 0, bool pk = false, bool notNull = false)
        : name(n), type(t), length(len), isPrimaryKey(pk), isNotNull(notNull) {}
};

class Schema {
public:
    Schema() = default;
    explicit Schema(const std::vector<Column>& cols) : columns(cols) {}
    const std::vector<Column>& GetColumns() const { return columns; }
    std::vector<Column>& MutableColumns() { return columns; }
    int  GetColumnCount() const { return (int)columns.size(); }
private:
    std::vector<Column> columns;
};

// 表信息：增加页链起止
struct TableInfo {
    std::string name;
    Schema      schema;
    std::string file_name;
    uint32_t    first_pid{ 0 };
    uint32_t    last_pid{ 0 };

    TableInfo(const std::string& n, 
              const Schema& s, 
              const std::string& f,
              uint32_t fp = 0, 
              uint32_t lp = 0)
        : name(n), schema(s), file_name(f), first_pid(fp), last_pid(lp) {}

    // —— 这三个 getter 恢复 —— //
    const std::string& getName() const { return name; }
    const Schema& getSchema() const { return schema; }
    const std::string& getFileName() const { return file_name; }
};

// Catalog：AddTable 支持 5 参（后 2 个有默认值，兼容旧调用）
class Catalog {
public:
    void AddTable(const std::string& name,
        const Schema& schema,
        const std::string& fileName,
        uint32_t first_pid = 0,
        uint32_t last_pid = 0)
    {
        tables[name] = std::make_unique<TableInfo>(name, schema, fileName,
            first_pid, last_pid);
    }

    TableInfo* GetTable(const std::string& name) {
        auto it = tables.find(name);
        return (it == tables.end()) ? nullptr : it->second.get();
    }

    // 新增：const 重载（解决“丢失限定符”）
    const TableInfo* GetTable(const std::string& name) const {
        auto it = tables.find(name);
        return (it == tables.end()) ? nullptr : it->second.get();
    }

    std::vector<std::string> ListTables() const {
        std::vector<std::string> v;
        v.reserve(tables.size());
        for (auto& kv : tables) v.push_back(kv.first);
        return v;
    }

    bool RemoveTable(const std::string& name) {
        return tables.erase(name) > 0;
    }

private:
    std::map<std::string, std::unique_ptr<TableInfo>> tables;
};

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

    // 新增：更新页链（存储层分配/扩展后回写）
    bool UpdateTablePages(Catalog& catalog,
        const std::string& tableName,
        uint32_t first_pid,
        uint32_t last_pid);

    // === 新增：把内存中的 Catalog 持久化到 catalog.txt ===
    // 若你已有 SaveCatalog/Flush，请把名字改成你现有的函数名，并在 StorageEngine 调用它。
    bool PersistCatalog(const Catalog& cat, const std::string& path = "catalog.txt");

    // --- NEW: 表重命名 ---
    bool RenameTable(Catalog& catalog,
        const std::string& oldName,
        const std::string& newName);

    // --- NEW: 增列（afterName为空则末尾添加）---
    bool AlterAddColumn(Catalog& catalog,
        const std::string& tableName,
        const Column& newCol,
        const std::string& afterName = "");

    // --- NEW: 删列 ---
    bool AlterDropColumn(Catalog& catalog,
        const std::string& tableName,
        const std::string& colName);

    // --- NEW: 修改列类型/长度 ---
    bool AlterModifyColumn(Catalog& catalog,
        const std::string& tableName,
        const std::string& colName,
        ColumnType newType,
        int newLen /*VARCHAR长度，其他类型填0*/);

    // --- NEW: 改名+改类型（等价 MySQL CHANGE oldName newName type）---
    bool AlterChangeColumn(Catalog& catalog,
        const std::string& tableName,
        const std::string& oldName,
        const Column& newDef);

private:
    std::string file_path;
};
