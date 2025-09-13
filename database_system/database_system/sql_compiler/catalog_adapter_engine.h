// =============================================
// sql_compiler/catalog_adapter_engine.h
// 修正版：实现 ICatalog 完整接口
// =============================================
#pragma once
#include "catalog_iface.h"                // 你项目里的 ICatalog 接口
#include "../engine/catalog_manager.hpp"  // 队友的 Catalog / CatalogManager
#include <optional>
#include <string>
#include <vector>

namespace minidb {

    inline ColumnType to_engine_type(DataType t) {
        return (t == DataType::INT32) ? ColumnType::INT : ColumnType::VARCHAR;
    }
    inline DataType to_compiler_type(ColumnType t) {
        return (t == ColumnType::INT) ? DataType::INT32 : DataType::VARCHAR;
    }

    class CatalogEngineAdapter : public ICatalog {
    public:
        CatalogEngineAdapter(CatalogManager& mgr, Catalog& cat)
            : mgr_(mgr), cat_(cat) {}

        // 注意：这里是 const 成员函数，签名需与 ICatalog 完全一致
        std::optional<TableDef> get_table(const std::string& name) const override {
            TableInfo* ti = const_cast<Catalog&>(cat_).GetTable(name);
            if (!ti) return std::nullopt;
            TableDef td;
            td.name = ti->getName();
            for (const auto& c : ti->getSchema().GetColumns()) {
                td.columns.push_back({ c.name, to_compiler_type(c.type) });
            }
            return td;
        }

        // 供语义阶段可能用到的建表（有的同学在语义里会检查/注册）
        Status create_table(const TableDef& tdef) override {
            std::vector<::Column> cols;  // 注意使用 ::Column（全局）
            cols.reserve(tdef.columns.size());
            for (auto& c : tdef.columns) {
                cols.emplace_back(c.name, to_engine_type(c.type), /*length*/64);
            }
            Schema schema(cols);
            bool ok = mgr_.CreateTable(cat_, tdef.name, schema, tdef.name + ".tbl");
            return ok ? Status::OK() : Status::Error("CreateTable failed in CatalogManager");
        }

    private:
        CatalogManager& mgr_;
        Catalog& cat_;
    };

} // namespace minidb
