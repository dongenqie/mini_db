// =============================================
// sql_compiler/catalog_iface.h
// =============================================
#pragma once
#include "../utils/common.h"
#include <unordered_map>

namespace minidb {

    // ����Ŀ¼�ӿڣ������������Ҫ����С��ṹ��ѯ/ע������
    class ICatalog {
    public:
        virtual ~ICatalog() = default;
        virtual std::optional<TableDef> get_table(const std::string& name) const = 0;
        virtual Status create_table(const TableDef& t) = 0;
    };

    // ���ڴ�ʵ�֣������� sql_compiler �������ԣ����־û���
    class InMemoryCatalog : public ICatalog {
    public:
        std::optional<TableDef> get_table(const std::string& name) const override {
            auto it = tables_.find(name);
            if (it == tables_.end()) return std::nullopt;
            return it->second;
        }
        Status create_table(const TableDef& t) override {
            if (tables_.count(t.name)) return Status::Error("Table exists");
            tables_[t.name] = t;
            return Status::OK();
        }
        // �����ã����
        void clear() { tables_.clear(); }
    private:
        std::unordered_map<std::string, TableDef> tables_;
    };

} // namespace minidb
