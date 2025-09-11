// =============================================
// sql_compiler/catalog_iface.h
// =============================================
#pragma once
#include "../utils/common.h"
#include <unordered_map>

namespace minidb {

    // 抽象目录接口（语义分析最小需求）
    class ICatalog {
    public:
        virtual ~ICatalog() = default;
        virtual std::optional<TableDef> get_table(const std::string& name) const = 0;
        virtual Status create_table(const TableDef& t) = 0;
    };

    // 仅用于 sql_compiler 独立联调的内存实现（大小写不敏感）
    class InMemoryCatalog : public ICatalog {
    public:
        std::optional<TableDef> get_table(const std::string& name) const override {
            auto it = tables_.find(to_lower(name));
            if (it == tables_.end()) return std::nullopt;
            return it->second;
        }
        Status create_table(const TableDef& t) override {
            std::string key = to_lower(t.name);
            if (tables_.count(key)) return Status::Error("Table exists");
            tables_[key] = t; // 实名保存在 TableDef.name 内
            return Status::OK();
        }
        void clear() { tables_.clear(); }
    private:
        std::unordered_map<std::string, TableDef> tables_; // key 为小写表名
    };

} // namespace minidb
