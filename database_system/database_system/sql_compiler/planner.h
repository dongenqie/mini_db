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

    // �ƻ���������
    enum class PlanOp {
        CREATE,     // CreateTable
        INSERT,     // Insert
        SEQSCAN,    // ȫ��ɨ��
        FILTER,     // ѡ��
        PROJECT,    // ͶӰ
        DELETE_,    // ɾ��
        ERROR       // ����ռλ
    };

    struct PlanNode {
        PlanOp op{ PlanOp::ERROR };

        // ͨ��
        std::vector<std::unique_ptr<PlanNode>> children;

        // === ��Բ�ͬ���ӵ���Ч�غ� ===
        // CREATE
        TableDef create_def;

        // SEQSCAN / DELETE / INSERT
        std::string table;

        // PROJECT
        std::vector<std::string> project;   // Ϊ�ձ�ʾ "*"

        // FILTER / DELETE ν��
        std::unique_ptr<Expr> predicate;

        // INSERT
        std::vector<std::string> insert_cols;                 // Ϊ�ձ�ʾ������˳��
        std::vector<std::unique_ptr<Expr>> insert_values;     // �� insert_cols ���루������ж��룩

        // ERROR
        std::string error_msg;
    };

    struct Plan {
        std::unique_ptr<PlanNode> root;
    };

    class Planner {
    public:
        // �� AST ���ɼƻ������ٶ�������ͨ����������������׳���жϣ�
        Plan plan_from_stmt(Stmt* s);

    private:
        std::unique_ptr<PlanNode> plan_create(const CreateTableStmt* s);
        std::unique_ptr<PlanNode> plan_insert(const InsertStmt* s);
        std::unique_ptr<PlanNode> plan_select(const SelectStmt* s);
        std::unique_ptr<PlanNode> plan_delete(const DeleteStmt* s);

        // С����
        static std::unique_ptr<PlanNode> make_error(std::string msg);
    };

} // namespace minidb
