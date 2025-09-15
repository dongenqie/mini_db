// =============================================
// sql_compiler/semantic.cpp
// =============================================
#include "semantic.h"
#include "../utils/common.h"   
#include <unordered_set>
#include <sstream>

// ---- 辅助：比较运算符转字符串 + 表达式转字符串（用于四元式打印） ----
namespace {
    using namespace minidb;

    static std::string cmpop_to_str(CmpOp op) {
        switch (op) {
        case CmpOp::EQ:  return "==";
        case CmpOp::NEQ: return "!=";
        case CmpOp::LT:  return "<";
        case CmpOp::LE:  return "<=";
        case CmpOp::GT:  return ">";
        case CmpOp::GE:  return ">=";
        }
        return "?";
    }

    // —— 表达式类型推断 —— //
// 支持：
//  - IntLit => INT32
//  - StrLit 若文本形如 123 或 66.66，可视作数字常量（用于与 FLOAT/DECIMAL 匹配）
//  - ColRef => 列定义的类型
    static bool is_integer_like(const std::string& s) {
        if (s.empty()) return false;
        size_t i = 0;
        if (s[0] == '+' || s[0] == '-') i++;
        if (i >= s.size()) return false;
        for (; i < s.size(); ++i) if (!std::isdigit((unsigned char)s[i])) return false;
        return true;
    }
    static bool is_decimal_like(const std::string& s) {
        if (s.empty()) return false;
        size_t i = 0; int dots = 0; int digits = 0;
        if (s[0] == '+' || s[0] == '-') i++;
        for (; i < s.size(); ++i) {
            if (s[i] == '.') { dots++; if (dots > 1) return false; }
            else if (std::isdigit((unsigned char)s[i])) { digits++; }
            else return false;
        }
        return digits > 0 && dots == 1;
    }

    // 你在 SELECT/DELETE/UPDATE 的四元式里调用的 render_expr 就是它
    static std::string render_expr(const Expr* e) {
        if (!e) return "-";
        if (auto c = dynamic_cast<const ColRef*>(e))  return c->name;
        if (auto i = dynamic_cast<const IntLit*>(e))  return std::to_string(i->v);
        if (auto s = dynamic_cast<const StrLit*>(e))  return "'" + s->v + "'";
        if (auto cmp = dynamic_cast<const CmpExpr*>(e)) {
            std::ostringstream os;
            os << "("
                << render_expr(cmp->lhs.get()) << " "
                << cmpop_to_str(cmp->op) << " "
                << render_expr(cmp->rhs.get()) << ")";
            return os.str();
        }
        return "expr";
    }
} // anonymous namespace

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
            return DataType::INT32; // bool
        }
        st = Status::Error("[SemanticError, expression, Unsupported expression]");
        return std::nullopt;
    }

    // —— CREATE —— //
    static const char* DtName(DataType dt) {
        switch (dt) {
        case DataType::INT32:     return "INT";
        case DataType::TINYINT:   return "TINYINT";
        case DataType::FLOAT:     return "FLOAT";
        case DataType::CHAR:      return "CHAR";
        case DataType::VARCHAR:   return "VARCHAR";
        case DataType::DECIMAL:   return "DECIMAL";
        case DataType::TIMESTAMP: return "TIMESTAMP";
        default:                  return "VARCHAR";
        }
    }
    Status SemanticAnalyzer::check_create(const CreateTableStmt* s, std::vector<Quad>& q) {
        if (cat_.get_table(to_lower(s->def.name)))
            return Status::Error("[SemanticError, table, Table already exists: " + s->def.name + "]");
        if (s->def.columns.empty())
            return Status::Error("[SemanticError, column, Empty column list]");

        std::unordered_set<std::string> seen;
        for (auto& c : s->def.columns) {
            std::string key = to_lower(c.name);
            if (!seen.insert(key).second)
                return Status::Error("[SemanticError, column, Duplicate column: " + c.name + "]");
        }

        std::vector<std::string> t_list;
        auto next_tmp = [&]() { return std::string("T") + std::to_string((int)q.size()); };

        for (auto& c : s->def.columns) {
            std::string t = next_tmp();
            q.push_back(Quad{ "COLDEF", c.name, DtName(c.type), t });
            t_list.push_back(t);
        }

        q.push_back(Quad{ "CREATE", s->def.name, "-", "-" });

        std::string arg1;
        for (size_t i = 0; i < t_list.size(); ++i) {
            if (i) arg1 += ",";
            arg1 += t_list[i];
        }
        q.push_back(Quad{ "RESULT", arg1, "-" });

        return Status::OK();
    }

    // —— INSERT —— //
    Status SemanticAnalyzer::check_insert(const InsertStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
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

        // 类型检查（INSERT 里允许未加引号的标识符作为 VARCHAR 字面量）+ 数值放宽
        auto is_numeric_compatible = [](DataType want, DataType got) {
            if (want == got) return true;
            // 允许 INT32 和 FLOAT/DECIMAL 互转（最小放宽）
            if ((want == DataType::INT32 || want == DataType::FLOAT || want == DataType::DECIMAL) &&
                (got == DataType::INT32 || got == DataType::FLOAT || got == DataType::DECIMAL))
                return true;
            return false;
            };

        for (size_t j = 0; j < s->values.size(); ++j) {
            const auto target_ty = td.columns[pos[j]].type;

            if (dynamic_cast<const ColRef*>(s->values[j].get())) {
                if (target_ty != DataType::VARCHAR) {
                    return Status::Error("[SemanticError, type, Type mismatch for column "
                        + td.columns[pos[j]].name + "]");
                }
                continue;
            }

            Status tmp = Status::OK();
            auto t = expr_type(s->values[j].get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || !is_numeric_compatible(target_ty, *t)) {
                return Status::Error("[SemanticError, type, Type mismatch for column "
                    + td.columns[pos[j]].name + "]");
            }
        }


        // 过程：对每个值先生成 CONST，拿到 T?；再按列生成 INSERT col, <literal>, T?
        auto next_tmp = [&]() {
            return std::string("T") + std::to_string((int)q.size());
            };
        std::vector<std::string> value_tmps;
        value_tmps.reserve(s->values.size());

        // 1) CONST 四元式：把字面量渲染到 arg1
        auto render_expr = [](const Expr* e) -> std::string {
            if (auto i = dynamic_cast<const IntLit*>(e))  return std::to_string(i->v);
            if (auto s = dynamic_cast<const StrLit*>(e))  return "'" + s->v + "'";
            if (auto c = dynamic_cast<const ColRef*>(e))  return c->name;
            return "expr";
            };

        for (size_t j = 0; j < s->values.size(); ++j) {
            std::string lit = render_expr(s->values[j].get());
            std::string t = next_tmp();
            q.push_back(Quad{ "CONST", lit, "-", t });
            value_tmps.push_back(t);
        }

        // 2) INSERT（列 ← 值T?）
        for (size_t j = 0; j < s->values.size(); ++j) {
            const auto& col = td.columns[pos[j]].name;
            std::string t = next_tmp();
            q.push_back(Quad{ "INSERT", col, value_tmps[j], t });
        }

        // 3) INTO：把所有值 T? 串起来
        std::string arg1;
        for (size_t i = 0; i < value_tmps.size(); ++i) {
            if (i) arg1 += ",";
            arg1 += value_tmps[i];
        }
        q.push_back(Quad{ "INTO", td.name, arg1, "-" });

        return Status::OK();
    }

    // —— SELECT（单表版：含 where/限定列名；* 支持） —— //
    Status SemanticAnalyzer::check_select(const SelectStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        auto td = *tdopt;

        // 记录每一步产生的临时名 T*
        std::vector<std::string> t_list;
        auto next_tmp = [&]() {
            return std::string("T") + std::to_string((int)q.size());
        };

        if (!s->star) {
            for (auto& c : s->columns) {
                if (!find_col_ci(td, c))
                    return Status::Error("[SemanticError, column, Unknown column: " + c + "]");
                std::string t = next_tmp();
                q.push_back(Quad{ "SELECT", c, "-", t });
                t_list.push_back(t);
            }
        }
        else {
            std::string t = next_tmp();
            q.push_back(Quad{ "SELECT", "*", "-", t });
            t_list.push_back(t);
        }

        // FROM
        {
            std::string t = next_tmp();
            q.push_back(Quad{ "FROM", td.name, "-", t });
            t_list.push_back(t);
        }

        // WHERE（如果有的话，先做类型检查，再记录谓词产生的 T）
        std::string pred_tmp;
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;

            // 渲染一个简洁的比较四元式
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
                pred_tmp = next_tmp();
                q.push_back(Quad{ op, a1, a2, pred_tmp });
            }
            else {
                pred_tmp = next_tmp();
                q.push_back(Quad{ "PRED", "expr", "-", pred_tmp });
            }
        }

        // 生成 RESULT：把 t_list 用逗号拼接；有 where 时追加 " WHERE T?"
        std::string arg1;
        for (size_t i = 0; i < t_list.size(); ++i) {
            if (i) arg1 += ",";
            arg1 += t_list[i];
        }
        if (!pred_tmp.empty()) {
            arg1 += " WHERE ";
            arg1 += pred_tmp;
        }
        q.push_back(Quad{ "RESULT", arg1, "-" });

        return Status::OK();
    }

    // —— DELETE（单表 where） —— //
    Status SemanticAnalyzer::check_delete(const DeleteStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        const TableDef& td = *tdopt;
        
        auto next_tmp = [&]() {
            return std::string("T") + std::to_string((int)q.size());
            };

        // FROM
        std::string t_from = next_tmp();
        q.push_back(Quad{ "FROM", td.name, "-", t_from });

        // WHERE（可选）
        std::string t_pred;
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;

            // 简洁比较四元式
            if (auto cmp = dynamic_cast<const CmpExpr*>(s->where.get())) {
                auto lhs = dynamic_cast<ColRef*>(cmp->lhs.get());
                auto rhs_i = dynamic_cast<IntLit*>(cmp->rhs.get());
                auto rhs_s = dynamic_cast<StrLit*>(cmp->rhs.get());
                std::string a1 = lhs ? lhs->name : "expr";
                std::string a2 = rhs_i ? std::to_string(rhs_i->v) : (rhs_s ? ("'" + rhs_s->v + "'") : "expr");
                std::string op =
                    (cmp->op == CmpOp::EQ ? "==" :
                        cmp->op == CmpOp::NEQ ? "!=" :
                        cmp->op == CmpOp::LT ? "<" :
                        cmp->op == CmpOp::LE ? "<=" :
                        cmp->op == CmpOp::GT ? ">" : ">=");
                t_pred = next_tmp();
                q.push_back(Quad{ op, a1, a2, t_pred });
            }
            else {
                t_pred = next_tmp();
                q.push_back(Quad{ "PRED", "expr", "-", t_pred });
            }
        }

        // RESULT：T_from [WHERE T_pred]
        std::string arg1 = t_from;
        if (!t_pred.empty()) {
            arg1 += " WHERE ";
            arg1 += t_pred;
        }
        q.push_back(Quad{ "RESULT", arg1, "-", "-" });

        // 最后给出删除动作
        q.push_back(Quad{ "DELETE", td.name, "-", "-" });
        return Status::OK();
    }

    // —— UPDATE —— //
    Status SemanticAnalyzer::check_update(const UpdateStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        const TableDef& td = *tdopt;

        auto is_numeric_compatible = [](DataType want, DataType got) {
            if (want == got) return true;
            if ((want == DataType::INT32 || want == DataType::FLOAT || want == DataType::DECIMAL) &&
                (got == DataType::INT32 || got == DataType::FLOAT || got == DataType::DECIMAL))
                return true;
            return false;
            };

        for (auto& kv : s->sets) {
            const std::string& col = kv.first;
            auto idx = find_col_ci(td, col);
            if (!idx) return Status::Error("[SemanticError, column, Unknown column: " + col + "]");
            Status tmp = Status::OK();
            auto t = expr_type(kv.second.get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || !is_numeric_compatible(td.columns[*idx].type, *t))
                return Status::Error("[SemanticError, type, Type mismatch for column " + td.columns[*idx].name + "]");

            q.push_back(Quad{ "SET", td.columns[*idx].name, render_expr(kv.second.get()), "-" });
        }


        // WHERE 表达式类型检查
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;
            q.push_back(Quad{ "WHERE", render_expr(s->where.get()), "-", "-" });
        }

        q.push_back(Quad{ "UPDATE", s->table, "-", "-" });
        return Status::OK();
    }

    // —— DROP —— //
    Status SemanticAnalyzer::check_drop(const DropTableStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt) {
            if (s->if_exists) { q.push_back(Quad{ "DROP", s->table, "IF EXISTS", "-" }); return Status::OK(); }
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        }
        q.push_back(Quad{ "DROP", s->table, "-", "-" });
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
        if (auto dr = dynamic_cast<DropTableStmt*>(s)) return { check_drop(dr, q), std::move(q) };
        return { Status::Error("[SemanticError, stmt, Unknown statement]"), {} };
    }

} // namespace minidb
