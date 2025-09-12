// =============================================
// sql_compiler/planner.h
// =============================================
#pragma once
#include "ast.h"
#include "../utils/common.h"
#include <memory>
#include <vector>
#include <string>

namespace minidb {

    // 计划算子类型
    enum class PlanOp {
        CREATE,     // CreateTable
        INSERT,     // Insert
        SEQSCAN,    // 全表扫描
        FILTER,     // 选择
        PROJECT,    // 投影
        DELETE_,    // 删除
        ERROR       // 出错占位
    };

    struct PlanNode {
        PlanOp op{ PlanOp::ERROR };

        // 通用
        std::vector<std::unique_ptr<PlanNode>> children;

        // === 针对不同算子的有效载荷 ===
        // CREATE
        TableDef create_def;

        // SEQSCAN / DELETE / INSERT
        std::string table;

        // PROJECT
        std::vector<std::string> project;   // 为空表示 "*"

        // FILTER / DELETE 谓词
        std::unique_ptr<Expr> predicate;

        // INSERT
        std::vector<std::string> insert_cols;                 // 为空表示按表列顺序
        std::vector<std::unique_ptr<Expr>> insert_values;     // 与 insert_cols 对齐（或与表列对齐）

        // ERROR
        std::string error_msg;
    };

    struct Plan {
        std::unique_ptr<PlanNode> root;
    };

    class Planner {
    public:
        // 从 AST 生成计划树（假定语义已通过；但仍做基本健壮性判断）
        Plan plan_from_stmt(Stmt* s);

    private:
        std::unique_ptr<PlanNode> plan_create(const CreateTableStmt* s);
        std::unique_ptr<PlanNode> plan_insert(const InsertStmt* s);
        std::unique_ptr<PlanNode> plan_select(const SelectStmt* s);
        std::unique_ptr<PlanNode> plan_delete(const DeleteStmt* s);

        // 小工具
        static std::unique_ptr<PlanNode> make_error(std::string msg);
    };

} // namespace minidb
