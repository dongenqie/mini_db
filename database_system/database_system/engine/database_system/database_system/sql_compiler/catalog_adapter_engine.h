// =============================================
// sql_compiler/catalog_adapter_engine.h
// =============================================
#pragma once
#include "catalog_iface.h"
// #include "../engine/catalog_manager.hpp"   // �ȶ���ʵ�ֺ�������

namespace minidb {

    // �� engine::CatalogManager ����� ICatalog
    class EngineCatalogAdapter /*: public ICatalog*/ {
    public:
        // ��ʽ������ʵĿ¼��δ����ע�ͣ�
        // explicit EngineCatalogAdapter(CatalogManager& impl) : impl_(impl) {}

        // std::optional<TableDef> get_table(const std::string& name) const override { return impl_.get_table(name); }
        // Status create_table(const TableDef& t) override { return impl_.create_table(t); }

    private:
        // CatalogManager& impl_;
    };

} // namespace minidb
