// =============================================
// sql_compiler/semantic.cpp
// =============================================
#include "semantic.h"
#include <unordered_set>

namespace minidb {

    static std::optional<size_t> find_col(const TableDef& td, const std::string& n) {
        for (size_t i = 0; i < td.columns.size(); ++i) if (td.columns[i].name == n) return i;
        return std::nullopt;
    }

    std::optional<DataType> SemanticAnalyzer::expr_type(const Expr* e, const TableDef& td, Status& st) {
        if (auto c = dynamic_cast<const ColRef*>(e)) {
            auto idx = find_col(td, c->name);
            if (!idx) { st = Status::Error("Unknown column: " + c->name); return std::nullopt; }
            return td.columns[*idx].type;
        }
        if (dynamic_cast<const IntLit*>(e)) return DataType::INT32;
        if (dynamic_cast<const StrLit*>(e)) return DataType::VARCHAR;

        if (auto cmp = dynamic_cast<const CmpExpr*>(e)) {
            auto lt = expr_type(cmp->lhs.get(), td, st); if (!st.ok) return std::nullopt;
            auto rt = expr_type(cmp->rhs.get(), td, st); if (!st.ok) return std::nullopt;
            if (lt != rt) { st = Status::Error("Type mismatch in comparison"); return std::nullopt; }
            return DataType::INT32; // ²¼¶ûÓÃ 0/1
        }

        st = Status::Error("Unsupported expr");
        return std::nullopt;
    }

    Status SemanticAnalyzer::check_create(const CreateTableStmt* s) {
        if (cat_.get_table(s->def.name)) return Status::Error("Table already exists: " + s->def.name);
        if (s->def.columns.empty())      return Status::Error("Empty column list");

        std::unordered_set<std::string> seen;
        for (auto& c : s->def.columns)
            if (!seen.insert(c.name).second) return Status::Error("Duplicate column: " + c.name);

        return Status::OK();
    }

    Status SemanticAnalyzer::check_insert(const InsertStmt* s) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt) return Status::Error("Unknown table: " + s->table);
        auto td = *tdopt;

        std::vector<size_t> pos;
        if (s->columns.empty()) {
            if (s->values.size() != td.columns.size()) return Status::Error("Values count mismatch");
            for (size_t i = 0; i < td.columns.size(); ++i) pos.push_back(i);
        }
        else {
            if (s->values.size() != s->columns.size()) return Status::Error("Columns vs values mismatch");
            for (auto& nm : s->columns) {
                auto idx = find_col(td, nm);
                if (!idx) return Status::Error("Unknown column: " + nm);
                pos.push_back(*idx);
            }
        }

        for (size_t j = 0; j < s->values.size(); ++j) {
            Status tmp = Status::OK();
            auto t = expr_type(s->values[j].get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || *t != td.columns[pos[j]].type)
                return Status::Error("Type mismatch for column " + td.columns[pos[j]].name);
        }
        return Status::OK();
    }

    Status SemanticAnalyzer::check_select(const SelectStmt* s) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt) return Status::Error("Unknown table: " + s->table);
        auto td = *tdopt;

        if (!s->star)
            for (auto& c : s->columns)
                if (!find_col(td, c)) return Status::Error("Unknown column: " + c);

        if (s->where) { Status st = Status::OK(); expr_type(s->where.get(), td, st); if (!st.ok) return st; }
        return Status::OK();
    }

    Status SemanticAnalyzer::check_delete(const DeleteStmt* s) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt) return Status::Error("Unknown table: " + s->table);
        if (s->where) { Status st = Status::OK(); expr_type(s->where.get(), *tdopt, st); if (!st.ok) return st; }
        return Status::OK();
    }

    SemanticResult SemanticAnalyzer::analyze(Stmt* s) {
        if (auto c = dynamic_cast<CreateTableStmt*>(s)) return { check_create(c) };
        if (auto i = dynamic_cast<InsertStmt*>(s))      return { check_insert(i) };
        if (auto q = dynamic_cast<SelectStmt*>(s))      return { check_select(q) };
        if (auto d = dynamic_cast<DeleteStmt*>(s))      return { check_delete(d) };
        return { Status::Error("Unknown stmt") };
    }

} // namespace minidb