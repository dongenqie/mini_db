// =============================================
// sql_compiler/planner.h
// =============================================
#pragma once
#include "ast.h"

namespace minidb {

    // 逻辑计划
    enum class PlanType { CREATE, INSERT, SEQSCAN, FILTER, PROJECT, DELETE };

    struct PlanNode {
        PlanType type;
        std::vector<std::unique_ptr<PlanNode>> children;

        // 各算子需要的字段
        TableDef create_def;                  // CREATE
        std::string table;                    // INSERT/SEQSCAN/DELETE
        std::vector<std::string> project_cols;// PROJECT
        std::vector<Value> insert_values;     // INSERT (已转为 Value)
        std::unique_ptr<Expr> filter;         // FILTER / DELETE 谓词
    };

    struct Plan { std::unique_ptr<PlanNode> root; };

    class Planner {
    public:
        Plan plan_from_stmt(Stmt* s); // AST -> 逻辑计划
    };

} // namespace minidb