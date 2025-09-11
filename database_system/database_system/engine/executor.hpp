#pragma once
#include <string>
#include <vector>
#include "catalog_manager.hpp"

// ==========================
// Executor thực thi các lệnh SQL đơn giản
// ==========================
class Executor {
public:
    explicit Executor(CatalogManager& manager, Catalog& catalog)
        : catalogManager(manager), catalog(catalog) {
    }

    // Thực thi một câu SQL dạng chuỗi
    bool Execute(const std::string& sql);

    // Public API (wrapper cho test)
    bool CreateTable(const std::string& tableName,
        const std::vector<Column>& columns,
        const std::string& fileName);
    bool Insert(const std::string& tableName,
        const std::vector<std::string>& values);
    bool Select(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::string& whereCol = "",
        const std::string& whereVal = "");
    bool Delete(const std::string& tableName,
        const std::string& whereCol = "",
        const std::string& whereVal = "");
    bool DropTable(const std::string& tableName);
    void ShowTables();

private:
    CatalogManager& catalogManager;
    Catalog& catalog;

    // Các hàm thực thi riêng
    bool ExecuteCreateTable(const std::string& tableName,
        const std::vector<Column>& columns,
        const std::string& fileName);
    bool ExecuteInsert(const std::string& tableName,
        const std::vector<std::string>& values);
    bool ExecuteSelect(const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::string& whereCol,
        const std::string& whereVal);
    bool ExecuteDelete(const std::string& tableName,
        const std::string& whereCol,
        const std::string& whereVal);
    bool ExecuteDropTable(const std::string& tableName);
    void ExecuteShowTables();
};
