// =============================================
// sql_compiler/ast.h
// =============================================
#pragma once
#include "../utils/common.h"
#include <memory>

namespace minidb {

    // ���ʽ����
    struct Expr { virtual ~Expr() = default; };
    // ��ʶ��������
    struct ColRef : Expr { std::string name; explicit ColRef(std::string n) : name(std::move(n)) {} };
    struct IntLit : Expr { int32_t v; explicit IntLit(int32_t x) : v(x) {} };
    struct StrLit : Expr { std::string v; explicit StrLit(std::string s) : v(std::move(s)) {} };

    // �Ƚϲ���
    enum class CmpOp { EQ, NEQ, LT, GT, LE, GE };
    struct CmpExpr : Expr {
        std::unique_ptr<Expr> lhs, rhs; CmpOp op;
        CmpExpr(std::unique_ptr<Expr> l, CmpOp o, std::unique_ptr<Expr> r)
            : lhs(std::move(l)), rhs(std::move(r)), op(o) {}
    };

    // ������
    struct Stmt { virtual ~Stmt() = default; };

    struct CreateTableStmt : Stmt { TableDef def; };

    struct InsertStmt : Stmt {
        std::string table;
        std::vector<std::string> columns;                 // ����Ϊ�գ���ʾȫ��
        std::vector<std::unique_ptr<Expr>> values;        // VALUES(...) �ڵı��ʽ������ֻ֧����������
    };

    struct DeleteStmt : Stmt {
        std::string table;
        std::unique_ptr<Expr> where;      // ��Ϊ��
    };

    // ===== ������SELECT ��չ =====
    struct OrderItem { std::string column; bool asc{ true }; };
    enum class JoinType { INNER, LEFT, RIGHT, FULL };
    struct SelectJoin {
        JoinType type{ JoinType::INNER };
        std::string table;      // �����ӱ��ַ������ɣ�
        std::string alias;      // ��ѡ����
        std::unique_ptr<Expr> on;
    };

    struct SelectStmt :Stmt {
        std::string table;                // ����
        std::string alias;                // ����������ɿգ�
        std::vector<std::string> columns; // ѡ�У�star=true ��ʾ *��
        bool star{ false };
        std::unique_ptr<Expr> where;      // WHERE���ɿգ�

        std::vector<SelectJoin> joins;    // JOIN �б�
        std::vector<std::string> group_by;
        std::unique_ptr<Expr> having;
        std::vector<OrderItem> order_by;
    };
    
    // ������UPDATE ���
    struct UpdateStmt : Stmt {
        std::string table;
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> sets;
        std::unique_ptr<Expr> where;
    };

    // ������DROP ���
    struct DropTableStmt : Stmt {
        std::string table;
        bool if_exists{ false };
    };

    using StmtPtr = std::unique_ptr<Stmt>;
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace minidb