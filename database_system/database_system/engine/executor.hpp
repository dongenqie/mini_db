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

    // Thực thi 1 câu lệnh SQL cơ bản
    bool Execute(const std::string& sql);

private:
    CatalogManager& catalogManager;
    Catalog& catalog;

    // Các hàm riêng để xử lý lệnh
    bool ExecuteCreateTable(const std::string& tableName, const std::vector<Column>& columns, const std::string& fileName);
    bool ExecuteDropTable(const std::string& tableName);
    void ExecuteShowTables();
};
