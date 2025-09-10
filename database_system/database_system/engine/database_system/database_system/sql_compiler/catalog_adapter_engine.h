// =============================================
// sql_compiler/catalog_adapter_engine.h
// =============================================
#pragma once
#include "catalog_iface.h"
// #include "../engine/catalog_manager.hpp"   // 等队友实现后再启用

namespace minidb {

    // 把 engine::CatalogManager 适配成 ICatalog
    class EngineCatalogAdapter /*: public ICatalog*/ {
    public:
        // 显式依赖真实目录（未来解注释）
        // explicit EngineCatalogAdapter(CatalogManager& impl) : impl_(impl) {}

        // std::optional<TableDef> get_table(const std::string& name) const override { return impl_.get_table(name); }
        // Status create_table(const TableDef& t) override { return impl_.create_table(t); }

    private:
        // CatalogManager& impl_;
    };

} // namespace minidb
