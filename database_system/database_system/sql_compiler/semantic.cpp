// =============================================
// sql_compiler/semantic.cpp
// =============================================
#include "semantic.h"
#include "../utils/common.h"   
#include <unordered_set>
#include <sstream>

// ---- �������Ƚ������ת�ַ��� + ���ʽת�ַ�����������Ԫʽ��ӡ�� ----
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

    // ���� SELECT/DELETE/UPDATE ����Ԫʽ����õ� render_expr ������
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

    // ������ȡ�еġ�Ҷ��������ȥ�� a.b ǰ׺��
    static std::string leaf_col(const std::string& s) {
        auto p = s.rfind('.');
        return (p == std::string::npos) ? s : s.substr(p + 1);
    }

    // �滻ԭ���� find_col����Ҷ������
    static std::optional<size_t> find_col(const TableDef& td, const std::string& n) {
        std::string leaf = leaf_col(n);
        for (size_t i = 0; i < td.columns.size(); ++i)
            if (td.columns[i].name == leaf) return i;
        return std::nullopt;
    }

    // ���� ���ʽ�����ƶ� ���� //
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

    // ���� CREATE ���� //
    Status SemanticAnalyzer::check_create(const CreateTableStmt* s, std::vector<Quad>& q) {
        if (cat_.get_table(to_lower(s->def.name)))
            return Status::Error("[SemanticError, table, Table already exists: " + s->def.name + "]");
        if (s->def.columns.empty())
            return Status::Error("[SemanticError, column, Empty column list]");

        // ����������Сд�����У�
        std::unordered_set<std::string> seen;
        for (auto& c : s->def.columns) {
            std::string key = to_lower(c.name);
            if (!seen.insert(key).second)
                return Status::Error("[SemanticError, column, Duplicate column: " + c.name + "]");
        }
        // ���̣�����в��� COLDEF���� CREATE���� RESULT ����
        std::vector<std::string> t_list;
        auto next_tmp = [&]() {
            return std::string("T") + std::to_string((int)q.size());
            };

        for (auto& c : s->def.columns) {
            std::string t = next_tmp();
            q.push_back(Quad{ "COLDEF",
                              c.name,
                              (c.type == DataType::INT32 ? "INT" : "VARCHAR"),
                              t });
            t_list.push_back(t);
        }

        q.push_back(Quad{ "CREATE", s->def.name, "-", "-" });

        // RESULT: T0,T1,... �����ж������
        std::string arg1;
        for (size_t i = 0; i < t_list.size(); ++i) {
            if (i) arg1 += ",";
            arg1 += t_list[i];
        }
        q.push_back(Quad{ "RESULT", arg1, "-" });

        return Status::OK();
    }

    // ���� INSERT ���� //
    Status SemanticAnalyzer::check_insert(const InsertStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        auto td = *tdopt;

        // �ж�λ��δ�����嵥 -> ˳��ȫ�У������嵥 -> ӳ��
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

        // ���ͼ�飨INSERT ������δ�����ŵı�ʶ����Ϊ VARCHAR ��������
        for (size_t j = 0; j < s->values.size(); ++j) {
            const auto target_ty = td.columns[pos[j]].type;

            // ��������ʶ����ColRef������ INSERT �ﾳ�°������� VARCHAR ������
            if (dynamic_cast<const ColRef*>(s->values[j].get())) {
                if (target_ty != DataType::VARCHAR) {
                    return Status::Error("[SemanticError, type, Type mismatch for column "
                        + td.columns[pos[j]].name + "]");
                }
                continue; // ͨ��
            }

            // ��������԰���������飨IntLit/StrLit/�Ƚϳ���ȣ�
            Status tmp = Status::OK();
            auto t = expr_type(s->values[j].get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || *t != target_ty) {
                return Status::Error("[SemanticError, type, Type mismatch for column "
                    + td.columns[pos[j]].name + "]");
            }
        }


        // ���̣���ÿ��ֵ������ CONST���õ� T?���ٰ������� INSERT col, <literal>, T?
        auto next_tmp = [&]() {
            return std::string("T") + std::to_string((int)q.size());
            };
        std::vector<std::string> value_tmps;
        value_tmps.reserve(s->values.size());

        // 1) CONST ��Ԫʽ������������Ⱦ�� arg1
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

        // 2) INSERT���� �� ֵT?��
        for (size_t j = 0; j < s->values.size(); ++j) {
            const auto& col = td.columns[pos[j]].name;
            std::string t = next_tmp();
            q.push_back(Quad{ "INSERT", col, value_tmps[j], t });
        }

        // 3) INTO��������ֵ T? ������
        std::string arg1;
        for (size_t i = 0; i < value_tmps.size(); ++i) {
            if (i) arg1 += ",";
            arg1 += value_tmps[i];
        }
        q.push_back(Quad{ "INTO", td.name, arg1, "-" });

        return Status::OK();
    }

    // ���� SELECT������棺�� where/�޶�������* ֧�֣� ���� //
    Status SemanticAnalyzer::check_select(const SelectStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        auto td = *tdopt;

        // ��¼ÿһ����������ʱ�� T*
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

        // WHERE������еĻ����������ͼ�飬�ټ�¼ν�ʲ����� T��
        std::string pred_tmp;
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;

            // ��Ⱦһ�����ıȽ���Ԫʽ
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

        // ���� RESULT���� t_list �ö���ƴ�ӣ��� where ʱ׷�� " WHERE T?"
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

    // ���� DELETE������ where�� ���� //
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

        // WHERE����ѡ��
        std::string t_pred;
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;

            // ���Ƚ���Ԫʽ
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

        // RESULT��T_from [WHERE T_pred]
        std::string arg1 = t_from;
        if (!t_pred.empty()) {
            arg1 += " WHERE ";
            arg1 += t_pred;
        }
        q.push_back(Quad{ "RESULT", arg1, "-", "-" });

        // ������ɾ������
        q.push_back(Quad{ "DELETE", td.name, "-", "-" });
        return Status::OK();
    }

    // ���� UPDATE ���� //
    Status SemanticAnalyzer::check_update(const UpdateStmt* s, std::vector<Quad>& q) {
        auto tdopt = cat_.get_table(to_lower(s->table));
        if (!tdopt)
            return Status::Error("[SemanticError, table, Unknown table: " + s->table + "]");
        const TableDef& td = *tdopt;

        // SET ��� + ��Ԫʽ
        for (auto& kv : s->sets) {
            const std::string& col = kv.first;
            auto idx = find_col_ci(td, col);
            if (!idx) return Status::Error("[SemanticError, column, Unknown column: " + col + "]");
            Status tmp = Status::OK();
            auto t = expr_type(kv.second.get(), td, tmp);
            if (!tmp.ok) return tmp;
            if (!t || *t != td.columns[*idx].type)
                return Status::Error("[SemanticError, type, Type mismatch for column " + td.columns[*idx].name + "]");

            // ��Ԫʽ��SET col = expr
            q.push_back(Quad{ "SET", td.columns[*idx].name, render_expr(kv.second.get()), "-" });
        }

        // WHERE ���ʽ���ͼ��
        if (s->where) {
            Status st = Status::OK();
            (void)expr_type(s->where.get(), td, st);
            if (!st.ok) return st;
            q.push_back(Quad{ "WHERE", render_expr(s->where.get()), "-", "-" });
        }

        q.push_back(Quad{ "UPDATE", s->table, "-", "-" });
        return Status::OK();
    }

    // ���� ������� ���� //
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
