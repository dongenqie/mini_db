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
        n->create_def = s->def;   // 直接带上表定义
        return n;
    }

    // INSERT
    std::unique_ptr<PlanNode> Planner::plan_insert(const InsertStmt* s) {
        if (!s) return make_error("null InsertStmt");
        auto n = std::make_unique<PlanNode>();
        n->op = PlanOp::INSERT;
        n->table = s->table;
        n->insert_cols = s->columns;  // 可能为空（表示按表定义顺序）
        // 深拷贝表达式（保持与 AST 分离；后续执行器可做求值）
        for (const auto& e : s->values) {
            if (!e) return make_error("INSERT contains null expr");
            // 这里只做浅层克隆：因为我们没有提供通用 clone，这里直接把所有权移过去也可
            // 但 plan_from_stmt 仍会保留 AST 用在 Pretty/打印，所以不能 move 原 AST。
            // 简易做法：复刻常见字面量与列名；复杂表达式保持为 ERROR。
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

        // 1) 扫描
        auto scan = std::make_unique<PlanNode>();
        scan->op = PlanOp::SEQSCAN;
        scan->table = s->table;

        std::unique_ptr<PlanNode> cur = std::move(scan);

        // 2) 谓词
        if (s->where) {
            auto fil = std::make_unique<PlanNode>();
            fil->op = PlanOp::FILTER;

            // 同样做最小“克隆”，以避免破坏 AST 所有权
            if (auto c = dynamic_cast<CmpExpr*>(s->where.get())) {
                // 深克隆 CmpExpr（支持列名/整型/字符串）
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
                // 其它表达式类型暂不支持
                return make_error("WHERE expr not supported in planner");
            }

            fil->children.push_back(std::move(cur));
            cur = std::move(fil);
        }

        // 3) 投影
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

    // 入口
    Plan Planner::plan_from_stmt(Stmt* s) {
        Plan p;
        if (auto c = dynamic_cast<CreateTableStmt*>(s)) { p.root = plan_create(c); return p; }
        if (auto i = dynamic_cast<InsertStmt*>(s)) { p.root = plan_insert(i); return p; }
        if (auto q = dynamic_cast<SelectStmt*>(s)) { p.root = plan_select(q); return p; }
        if (auto d = dynamic_cast<DeleteStmt*>(s)) { p.root = plan_delete(d); return p; }
        p.root = make_error("Unsupported statement type in planner");
        return p;
    }

} // namespace minidb
