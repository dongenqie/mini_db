// =============================================
// sql_compiler/planner.cpp
// =============================================
#include "planner.hpp"

namespace minidb {

    Plan Planner::plan_from_stmt(Stmt* s) {
        Plan p;
        auto root = std::make_unique<PlanNode>();

        if (auto c = dynamic_cast<CreateTableStmt*>(s)) {
            root->type = PlanType::CREATE;
            root->create_def = c->def;
        }
        else if (auto i = dynamic_cast<InsertStmt*>(s)) {
            root->type = PlanType::INSERT;
            root->table = i->table;
            root->project_cols = i->columns; // 可能为空：表示全列
            for (auto& v : i->values) {
                if (auto il = dynamic_cast<IntLit*>(v.get())) root->insert_values.push_back(il->v);
                else if (auto sl = dynamic_cast<StrLit*>(v.get())) root->insert_values.push_back(sl->v);
            }
        }
        else if (auto q = dynamic_cast<SelectStmt*>(s)) {
            // scan -> [filter] -> project
            auto scan = std::make_unique<PlanNode>();
            scan->type = PlanType::SEQSCAN;
            scan->table = q->table;

            std::unique_ptr<PlanNode> cur = std::move(scan);

            if (q->where) {
                auto fil = std::make_unique<PlanNode>();
                fil->type = PlanType::FILTER;
                fil->filter = std::move(q->where);
                fil->children.push_back(std::move(cur));
                cur = std::move(fil);
            }

            auto proj = std::make_unique<PlanNode>();
            proj->type = PlanType::PROJECT;
            proj->project_cols = q->star ? std::vector<std::string>{} : q->columns;
            proj->children.push_back(std::move(cur));

            root = std::move(proj);
        }
        else if (auto d = dynamic_cast<DeleteStmt*>(s)) {
            root->type = PlanType::DELETE;
            root->table = d->table;
            root->filter = std::move(d->where);
        }

        p.root = std::move(root);
        return p;
    }

} // namespace minidb