// =============================================
// engine/executor.hpp  （覆盖/补丁）
// =============================================
#pragma once
#include <string>
#include <vector>
#include "catalog_manager.hpp"
#include "storage_engine.hpp"
#include "../sql_compiler/planner.h"
#include "../sql_compiler/ast.h"

class Executor {
public:
    Executor(CatalogManager& manager, Catalog& catalog, StorageEngine& storage)
        : catalogManager(manager), catalog(catalog), storage(storage) {}

    bool ExecutePlan(const minidb::Plan& plan);
    bool Execute(const std::string& sql);

private:
    CatalogManager& catalogManager;
    Catalog& catalog;
    StorageEngine& storage;

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
    bool ExecuteDropTable(const std::string& tableName, bool if_exists = false);
    void ExecuteShowTables();
};
