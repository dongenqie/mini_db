// =============================================
// sql_compiler/planner.cpp
// =============================================
#include "planner.h"

namespace minidb {

    std::unique_ptr<PlanNode> Planner::make_error(std::string msg) {
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::ERROR;
        n->error_msg = std::move(msg);
        return n;
    }

    // CREATE TABLE
    std::unique_ptr<PlanNode> Planner::plan_create(const CreateTableStmt* s) {
        if (!s) return make_error("null CreateTableStmt");
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::CREATE;
        n->create_def = s->def;   // ֱ�Ӵ��ϱ���
        return n;
    }

    // INSERT
    std::unique_ptr<PlanNode> Planner::plan_insert(const InsertStmt* s) {
        if (!s) return make_error("null InsertStmt");
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::INSERT;
        n->table = s->table;
        n->insert_cols = s->columns;  // ����Ϊ�գ���ʾ������˳��
        // ������ʽ�������� AST ���룻����ִ����������ֵ��
        for (const auto& e : s->values) {
            if (!e) return make_error("INSERT contains null expr");
            // ����ֻ��ǳ���¡����Ϊ����û���ṩͨ�� clone������ֱ�Ӱ�����Ȩ�ƹ�ȥҲ��
            // �� plan_from_stmt �Իᱣ�� AST ���� Pretty/��ӡ�����Բ��� move ԭ AST��
            // �������������̳��������������������ӱ��ʽ����Ϊ ERROR��
            if (auto c = dynamic_cast<ColRef*>(e.get())) {
                n->insert_values.emplace_back(std::make_unique<ColRef>(c->name));
            }
            else if (auto i = dynamic_cast<IntLit*>(e.get())) {
                n->insert_values.emplace_back(std::make_unique<IntLit>(i->v));
            }
            else if (auto st = dynamic_cast<StrLit*>(e.get())) {
                n->insert_values.emplace_back(std::make_unique<StrLit>(st->v));
            }
            else {
                return make_error("INSERT expr not supported in planner");
            }
        }
        return n;
    }

    // SELECT -> Project( Filter?( SeqScan(table) ) )
    std::unique_ptr<PlanNode> Planner::plan_select(const SelectStmt* s) {
        if (!s) return make_error("null SelectStmt");

        // 1) ɨ��
        auto scan = std::make_unique<PlanNode>();
        scan->op = PlanOp::SEQSCAN;
        scan->table = s->table;

        std::unique_ptr<PlanNode> cur = std::move(scan);

        // 2) ν��
        if (s->where) {
            auto fil = std::make_unique<PlanNode>();
            fil->op = PlanOp::FILTER;

            // ͬ������С����¡�����Ա����ƻ� AST ����Ȩ
            if (auto c = dynamic_cast<CmpExpr*>(s->where.get())) {
                // ���¡ CmpExpr��֧������/����/�ַ�����
                auto clone_side = [](const Expr* e) -> std::unique_ptr<Expr> {
                    if (auto x = dynamic_cast<const ColRef*>(e)) return std::make_unique<ColRef>(x->name);
                    if (auto x = dynamic_cast<const IntLit*>(e)) return std::make_unique<IntLit>(x->v);
                    if (auto x = dynamic_cast<const StrLit*>(e)) return std::make_unique<StrLit>(x->v);
                    return nullptr;
                    };
                auto L = clone_side(c->lhs.get());
                auto R = clone_side(c->rhs.get());
                if (!L || !R) return make_error("WHERE expr not supported in planner");
                fil->predicate = std::make_unique<CmpExpr>(std::move(L), c->op, std::move(R));
            }
            else {
                // �������ʽ�����ݲ�֧��
                return make_error("WHERE expr not supported in planner");
            }

            fil->children.push_back(std::move(cur));
            cur = std::move(fil);
        }

        // 3) ͶӰ
        auto proj = std::make_unique<PlanNode>();
        proj->op = PlanOp::PROJECT;
        proj->project = s->star ? std::vector<std::string>{} : s->columns;
        proj->children.push_back(std::move(cur));
        return proj;
    }

    // DELETE
    std::unique_ptr<PlanNode> Planner::plan_delete(const DeleteStmt* s) {
        if (!s) return make_error("null DeleteStmt");
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::DELETE_;
        n->table = s->table;

        if (s->where) {
            if (auto c = dynamic_cast<CmpExpr*>(s->where.get())) {
                auto clone_side = [](const Expr* e) -> std::unique_ptr<Expr> {
                    if (auto x = dynamic_cast<const ColRef*>(e)) return std::make_unique<ColRef>(x->name);
                    if (auto x = dynamic_cast<const IntLit*>(e)) return std::make_unique<IntLit>(x->v);
                    if (auto x = dynamic_cast<const StrLit*>(e)) return std::make_unique<StrLit>(x->v);
                    return nullptr;
                    };
                auto L = clone_side(c->lhs.get());
                auto R = clone_side(c->rhs.get());
                if (!L || !R) return make_error("DELETE WHERE expr not supported in planner");
                n->predicate = std::make_unique<CmpExpr>(std::move(L), c->op, std::move(R));
            }
            else {
                return make_error("DELETE WHERE expr not supported in planner");
            }
        }
        return n;
    }

    // DROP
    std::unique_ptr<PlanNode> Planner::plan_drop(const DropTableStmt* s) {
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::DROP;
        n->table = s->table;
        n->if_exists = s->if_exists;
        return n;
    }

    // UPDATE
    std::unique_ptr<PlanNode> Planner::plan_update(const UpdateStmt* s) {
        if (!s) return make_error("null UpdateStmt");
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::UPDATE;
        n->table = s->table;

        // ���� SET �б���֧�� ColRef/IntLit/StrLit��
        for (const auto& kv : s->sets) {
            const auto& col = kv.first;
            const Expr* e = kv.second.get();
            std::unique_ptr<Expr> val;
            if (auto ii = dynamic_cast<const IntLit*>(e))      val = std::make_unique<IntLit>(ii->v);
            else if (auto ss = dynamic_cast<const StrLit*>(e)) val = std::make_unique<StrLit>(ss->v);
            else if (auto cc = dynamic_cast<const ColRef*>(e)) val = std::make_unique<ColRef>(cc->name);
            else return make_error("UPDATE value expr not supported in planner");
            n->update_sets.emplace_back(col, std::move(val));
        }

        // WHERE����֧�ּ򵥱Ƚ� CmpExpr ��������������
        if (s->where) {
            if (auto c = dynamic_cast<CmpExpr*>(s->where.get())) {
                auto clone_side = [](const Expr* e)->std::unique_ptr<Expr> {
                    if (auto x = dynamic_cast<const ColRef*>(e)) return std::make_unique<ColRef>(x->name);
                    if (auto x = dynamic_cast<const IntLit*>(e)) return std::make_unique<IntLit>(x->v);
                    if (auto x = dynamic_cast<const StrLit*>(e)) return std::make_unique<StrLit>(x->v);
                    return nullptr;
                    };
                auto L = clone_side(c->lhs.get());
                auto R = clone_side(c->rhs.get());
                if (!L || !R) return make_error("UPDATE WHERE expr not supported in planner");
                n->predicate = std::make_unique<CmpExpr>(std::move(L), c->op, std::move(R));
            }
            else {
                return make_error("UPDATE WHERE expr not supported in planner");
            }
        }
        return n;
    }


    // ���
    Plan Planner::plan_from_stmt(Stmt* s) {
        Plan p;
        if (auto c = dynamic_cast<CreateTableStmt*>(s)) { p.root = plan_create(c); return p; }
        if (auto i = dynamic_cast<InsertStmt*>(s)) { p.root = plan_insert(i); return p; }
        if (auto q = dynamic_cast<SelectStmt*>(s)) { p.root = plan_select(q); return p; }
        if (auto d = dynamic_cast<DeleteStmt*>(s)) { p.root = plan_delete(d); return p; }
        if (auto dr = dynamic_cast<DropTableStmt*>(s)) { p.root = plan_drop(dr);  return p; }
        if (auto u = dynamic_cast<UpdateStmt*>(s)) { p.root = plan_update(u); return p; }
        p.root = make_error("Unsupported statement type in planner");
        return p;
    }

} // namespace minidb
