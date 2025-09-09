// =============================================
// sql_compiler/planner.h
// =============================================
#pragma once
#include "ast.h"

namespace minidb {

    // �߼��ƻ�
    enum class PlanType { CREATE, INSERT, SEQSCAN, FILTER, PROJECT, DELETE };

    struct PlanNode {
        PlanType type;
        std::vector<std::unique_ptr<PlanNode>> children;

        // ��������Ҫ���ֶ�
        TableDef create_def;                  // CREATE
        std::string table;                    // INSERT/SEQSCAN/DELETE
        std::vector<std::string> project_cols;// PROJECT
        std::vector<Value> insert_values;     // INSERT (��תΪ Value)
        std::unique_ptr<Expr> filter;         // FILTER / DELETE ν��
    };

    struct Plan { std::unique_ptr<PlanNode> root; };

    class Planner {
    public:
        Plan plan_from_stmt(Stmt* s); // AST -> �߼��ƻ�
    };

} // namespace minidb