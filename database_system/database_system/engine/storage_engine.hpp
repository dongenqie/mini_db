// =============================================
// engine/storage_engine.hpp  （新增/覆盖）
// =============================================
#pragma once
#include <string>
#include <vector>
#include "catalog_manager.hpp"
#include "../storage/file_manager.hpp"
#include "../storage/page.hpp"
#include <functional> 

class StorageEngine {
public:

    StorageEngine(CatalogManager& cmgr, Catalog& cat, FileManager& fm)
        : cmgr_(cmgr), catalog_(cat), fm_(fm) {}

    // 初始化表的链表（分配首个数据页）
    bool InitTablePages(const std::string& tableName);

    // 追加一条记录（values 已为字符串，如 "1", "Alice"）
    bool Insert(const std::string& tableName, const std::vector<std::string>& values);

    // 全表顺序扫描（按之前 CSV 打印风格返回）
    std::vector<std::vector<std::string>> SelectAll(const std::string& tableName);

    // 条件删除：whereColIndex == -1 表示全删
    bool DeleteWhere(const std::string& tableName, int whereColIndex, const std::string& whereVal);

    // 物理丢弃：释放该表所有页（仅演示，简单回收到 free list）
    bool DropTableData(const std::string& tableName);

    // 条件更新：将满足 where 条件的记录按 sets 更新
    //  whereColIndex: 参与条件的列索引；若是 IN/BETWEEN，请传 -2 并依赖谓词回调
    //  返回 true 表示成功
    bool UpdateWhere(
        const std::string& tableName,
        const std::function<bool(const std::vector<std::string>&)>& pred,
        const std::vector<std::pair<int, std::string>>& sets_by_idx);

    bool OverwriteAll(
        const std::string& tableName,
        const std::vector<std::vector<std::string>>& rows);

    // TRUNCATE：清空表数据并重建空页链（不删除表定义）
    bool TruncateTable(const std::string& tableName);

private:
    // --- 工具 ---
    static std::string join_csv(const std::vector<std::string>& values);
    static void split_csv_line(const std::string& line, std::vector<std::string>& out);

    bool append_to_page(uint32_t page_id, const std::string& line, uint32_t& new_free_off, bool& ok_written);
    bool allocate_linked_page(uint32_t prev_pid, uint32_t& new_pid);
    bool ensure_table_ready(const std::string& tableName);  // 新增：确保表有可用数据页

    // 遍历所有链页
    template<typename Fn>
    bool for_each_page(const TableInfo* t, Fn fn);

private:
    CatalogManager& cmgr_;
    Catalog& catalog_;
    FileManager& fm_;
};
