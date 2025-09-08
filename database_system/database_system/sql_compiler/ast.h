// =============================================
// sql_compiler/ast.hpp
// =============================================
#pragma once
#include "../utils/common.hpp"
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

    struct SelectStmt : Stmt {
        std::string table;
        std::vector<std::string> columns; // star==true ʱ����
        bool star{ false };
        std::unique_ptr<Expr> where;      // ��Ϊ��
    };

    struct DeleteStmt : Stmt {
        std::string table;
        std::unique_ptr<Expr> where;      // ��Ϊ��
    };

    using StmtPtr = std::unique_ptr<Stmt>;
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace minidb