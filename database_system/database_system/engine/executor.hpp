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

    bool Execute(const std::string& sql);
    bool ExecutePlan(const minidb::Plan& plan);

    // —— NEW: 当前数据库（默认 default）——
    void SetDatabase(const std::string& db) { current_db = db; }
    const std::string& CurrentDatabase() const { return current_db; }

public:
    CatalogManager& catalogManager;
    Catalog& catalog;
    StorageEngine& storage;

    // 原有成员：catalogManager, catalog, storage ...
    std::string current_db{ "default" }; // NEW: 当前库名

    // —— NEW: 数据库管理 —— 
    bool ExecuteCreateDatabase(const std::string& db);
    bool ExecuteDropDatabase(const std::string& db);
    bool ExecuteUseDatabase(const std::string& db);

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
    // DESC / DESCRIBE

    bool ExecuteDesc(const std::string& tableName);
    // SHOW CREATE TABLE
    bool ExecuteShowCreate(const std::string& tableName);
    bool ExecuteShowDatabases();

    // ALTER TABLE 分支
    bool ExecuteAlterRename(const std::string& oldName, const std::string& newName);
    bool ExecuteAlterAdd(const std::string& tableName, const Column& col, const std::string& after = "");
    bool ExecuteAlterDrop(const std::string& tableName, const std::string& colName);
    bool ExecuteAlterModify(const std::string& tableName, const std::string& colName, ColumnType ty, int len);
    bool ExecuteAlterChange(const std::string& tableName, const std::string& oldName, const Column& newDef);

    // 同时新增一个切库后的重绑定：
    bool RebindToCurrentDatabase(); // 根据 current_db 重新绑定 Catalog/Storage

    // engine/executor.hpp  （在 public 区域追加声明）
    bool ExecuteUpdate(
        const std::string& tableName,
        const std::vector<std::pair<std::string, std::string>>& sets,
        // where 三种形式：EQ / IN / BETWEEN（简化仅对单列）
        const std::string& whereCol,
        const std::vector<std::string>& whereInVals,
        const std::pair<std::string, std::string>& whereBetween, // [lo, hi]
        const std::string& whereEqVal,                          // 若启用 EQ
        int whereMode /*0=NONE,1=EQ,2=IN,3=BETWEEN*/);

    bool ExecuteTruncate(const std::string& tableName);


    void ExecuteShowTables();


};
