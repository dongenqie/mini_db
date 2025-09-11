// =============================================
// sql_compiler/semantic.cpp
// =============================================
#include "semantic.h"
#include <unordered_set>

namespace minidb {

    // 新增：取列的“叶子名”（去掉 a.b 前缀）
    static std::string leaf_col(const std::string& s) {
        auto p = s.rfind('.');
        return (p == std::string::npos) ? s : s.substr(p + 1);
    }

    // 替换原来的 find_col：按叶子名找
    static std::optional<size_t> find_col(const TableDef& td, const std::string& n) {
        std::string leaf = leaf_col(n);
        for (size_t i = 0; i < td.columns.size(); ++i)
            if (td.columns[i].name == leaf) return i;
        return std::nullopt;
    }

    // —— 表达式类型推断 —— //
    std::optional<DataType> SemanticAnalyzer::expr_type(const Expr* e, const TableDef& td, Status& st) {
        if (auto c = dynamic_cast<const ColRef*>(e)) {
            auto idx = find_col_ci(td, c->name);
            if (!idx) {
                st = Status::Error("[SemanticError, column, Unknown column: " + c->name + "]");
                return std::nullopt;
            }
            return td.columns[*idx].type;
        }
        if (dynamic_cast<const IntLit*>(e)) return DataType::INT32;
        if (dynamic_cast<const StrLit*>(e)) return DataType::VARCHAR;

        if (auto cmp = dynamic_cast<const CmpExpr*>(e)) {
            auto lt = expr_type(cmp->lhs.get(), td, st); if (!st.ok) return std::nullopt;
            auto rt = expr_type(cmp->rhs.get(), td, st); if (!st.ok) return std::nullopt;
            if (lt != rt) {
                st = Status::Error("[SemanticError, expression, Type mismatch in comparison]");
                return std::nullopt;
            }
            // 谓词一律归为 INT32(0/1)，便于后续阶段处理
            return DataType::INT32;
        }
        st = Status::Error("[SemanticError, expression, Unsupported expression]");
        return std::nullopt;
    }

    // —— CREATE —— //
    Status SemanticAnalyzer::check_create(const CreateTableStmt* s, std::vector<Quad>& q) {
        if (cat_.get_table(s->def.name))
            return Status::Error("[SemanticError, table, Table already exists: " + s->def.name + "]");
        if (s->def.columns.empty())
            return Status::Error("[SemanticError, column, Empty column list]");

        // 列重名（大小写不敏感）
        std::unordered_set<std::string> seen;
        for (auto& c : s->def.columns) {
            std::string key = to_lower(c.name);
            if (!seen.insert(key).second)
                return Status::Error("[SemanticError, column, Duplicate column: " + c.name + "]");
        }
        q.push_back({ "CREATE", s->def.name, "-", "-" });
        return Status::OK();
    }

    // —— INSERT —— //
    Status SemanticAnalyzer::check_insert(const InsertStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        auto td = *tdopt;

        // 列定位：未给列清单 -> 顺序全列；给列清单 -> 映射
        std::vector<size_t> pos;
        if (s->columns.empty()) {
            if (s->values.size() != td.columns.size())
                return Status::Error("[SemanticError, values, Values count mismatch: expected "
                    + std::to_string(td.columns.size()) + ", got " + std::to_string(s->values.size()) + "]");
            for (size_t i = 0; i < td.columns.size(); ++i) pos.push_back(i);
        }
        else {
            if (s->values.size() != s->columns.size())
                return Status::Error("[SemanticError, values, Columns vs values count mismatch]");
            for (auto& nm : s->columns) {
                auto idx = find_col_ci(td, nm);
                if (!idx) return Status::Error("[SemanticError, column, Unknown column: " + nm + "]");
                pos.push_back(*idx);
            }
        }

        // 类型检查（要求解析阶段把值解析成 Expr，现已满足）
        for (size_t j = 0; j < s->values.size(); ++j) {
            Status tmp = Status::OK();
            auto t = expr_type(s->values[j].get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || *t != td.columns[pos[j]].type) {
                return Status::Error("[SemanticError, type, Type mismatch for column "
                    + td.columns[pos[j]].name + "]");
            }
        }

        // 展示四元式
        for (size_t j = 0; j < s->values.size(); ++j) {
            q.push_back({ "INSERT", td.columns[pos[j]].name,
                         (td.columns[pos[j]].type == DataType::INT32 ? "INT" : "VARCHAR"),
                         "T" + std::to_string((int)j) });
        }
        q.push_back({ "INTO", td.name, "-", "-" });
        return Status::OK();
    }

    // —— SELECT（单表版：含 where/限定列名；* 支持） —— //
    Status SemanticAnalyzer::check_select(const SelectStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        auto td = *tdopt;

        if (!s->star) {
            for (auto& c : s->columns) {
                if (!find_col_ci(td, c))
                    return Status::Error("[SemanticError, column, Unknown column: " + c + "]");
                q.push_back({ "SELECT", c, "-", "T" + std::to_string((int)q.size()) });
            }
        }
        else {
            q.push_back({ "SELECT", "*", "-", "T0" });
        }

        q.push_back({ "FROM", td.name, "-", "T" + std::to_string((int)q.size()) });

        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;

            // 简洁的四元式渲染
            if (auto cmp = dynamic_cast<const CmpExpr*>(s->where.get())) {
                auto lhs = dynamic_cast<ColRef*>(cmp->lhs.get());
                auto rhs_i = dynamic_cast<IntLit*>(cmp->rhs.get());
                auto rhs_s = dynamic_cast<StrLit*>(cmp->rhs.get());
                std::string a1 = lhs ? lhs->name : "expr";
                std::string a2 = rhs_i ? std::to_string(rhs_i->v) : (rhs_s ? rhs_s->v : "expr");
                std::string op =
                    (cmp->op == CmpOp::EQ ? "==" :
                        cmp->op == CmpOp::NEQ ? "!=" :
                        cmp->op == CmpOp::LT ? "<" :
                        cmp->op == CmpOp::LE ? "<=" :
                        cmp->op == CmpOp::GT ? ">" : ">=");
                q.push_back({ op, a1, a2, "T" + std::to_string((int)q.size()) });
            }
            else {
                q.push_back({ "PRED", "expr", "-", "T" + std::to_string((int)q.size()) });
            }
        }

        q.push_back({ "RESULT", "-", "-", "-" });
        return Status::OK();
    }

    // —— DELETE（单表 where） —— //
    Status SemanticAnalyzer::check_delete(const DeleteStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), *tdopt, st);
            if (!st.ok) return st;
            q.push_back({ "WHERE", "predicate", "-", "-" });
        }
        q.push_back({ "DELETE", s->table, "-", "-" });
        return Status::OK();
    }

    // —— UPDATE —— //
    Status SemanticAnalyzer::check_update(const UpdateStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(s->table);
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        const TableDef& td = *tdopt;

        // SET 列存在与类型检查
        for (auto& kv : s->sets) {
            const std::string& col = kv.first;
            auto idx = find_col_ci(td, col);
            if (!idx) return Status::Error("[SemanticError, column, Unknown column: " + col + "]");
            Status tmp = Status::OK();
            auto t = expr_type(kv.second.get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || *t != td.columns[*idx].type)
                return Status::Error("[SemanticError, type, Type mismatch for column " + td.columns[*idx].name + "]");
        }

        // WHERE 表达式类型检查
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;
        }
        return Status::OK();
    }

    // —— 顶层调度 —— //
    SemanticResult SemanticAnalyzer::analyze(Stmt* s) {
        std::vector<Quad> q;
        if (auto c = dynamic_cast<CreateTableStmt*>(s)) return { check_create(c,q), std::move(q) };
        if (auto i = dynamic_cast<InsertStmt*>(s))      return { check_insert(i,q), std::move(q) };
        if (auto se = dynamic_cast<SelectStmt*>(s))      return { check_select(se,q), std::move(q) };
        if (auto d = dynamic_cast<DeleteStmt*>(s))      return { check_delete(d,q), std::move(q) };
        if (auto u = dynamic_cast<UpdateStmt*>(s))      return { check_update(u, q), std::move(q) };
        return { Status::Error("[SemanticError, stmt, Unknown statement]"), {} };
    }

} // namespace minidb