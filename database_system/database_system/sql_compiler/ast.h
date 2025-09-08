// =============================================
// sql_compiler/ast.hpp
// =============================================
#pragma once
#include "../utils/common.hpp"
#include <memory>

namespace minidb {

    // 表达式基类
    struct Expr { virtual ~Expr() = default; };

    // 标识符、常量
    struct ColRef : Expr { std::string name; explicit ColRef(std::string n) : name(std::move(n)) {} };
    struct IntLit : Expr { int32_t v; explicit IntLit(int32_t x) : v(x) {} };
    struct StrLit : Expr { std::string v; explicit StrLit(std::string s) : v(std::move(s)) {} };

    // 比较操作
    enum class CmpOp { EQ, NEQ, LT, GT, LE, GE };
    struct CmpExpr : Expr {
        std::unique_ptr<Expr> lhs, rhs; CmpOp op;
        CmpExpr(std::unique_ptr<Expr> l, CmpOp o, std::unique_ptr<Expr> r)
            : lhs(std::move(l)), rhs(std::move(r)), op(o) {}
    };

    // 语句基类
    struct Stmt { virtual ~Stmt() = default; };

    struct CreateTableStmt : Stmt { TableDef def; };

    struct InsertStmt : Stmt {
        std::string table;
        std::vector<std::string> columns;                 // 可能为空，表示全列
        std::vector<std::unique_ptr<Expr>> values;        // VALUES(...) 内的表达式（这里只支持字面量）
    };

    struct SelectStmt : Stmt {
        std::string table;
        std::vector<std::string> columns; // star==true 时忽略
        bool star{ false };
        std::unique_ptr<Expr> where;      // 可为空
    };

    struct DeleteStmt : Stmt {
        std::string table;
        std::unique_ptr<Expr> where;      // 可为空
    };

    using StmtPtr = std::unique_ptr<Stmt>;
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace minidb