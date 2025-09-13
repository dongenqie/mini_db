// =============================================
// sql_compiler/ast.h
// =============================================
#pragma once
#include "../utils/common.h"
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

    struct DeleteStmt : Stmt {
        std::string table;
        std::unique_ptr<Expr> where;      // 可为空
    };

    // ===== 新增：SELECT 扩展 =====
    struct OrderItem { std::string column; bool asc{ true }; };
    enum class JoinType { INNER, LEFT, RIGHT, FULL };
    struct SelectJoin {
        JoinType type{ JoinType::INNER };
        std::string table;      // 被连接表（字符串即可）
        std::string alias;      // 可选别名
        std::unique_ptr<Expr> on;
    };

    struct SelectStmt :Stmt {
        std::string table;                // 主表
        std::string alias;                // 主表别名（可空）
        std::vector<std::string> columns; // 选列（star=true 表示 *）
        bool star{ false };
        std::unique_ptr<Expr> where;      // WHERE（可空）

        std::vector<SelectJoin> joins;    // JOIN 列表
        std::vector<std::string> group_by;
        std::unique_ptr<Expr> having;
        std::vector<OrderItem> order_by;
    };
    
    // 新增：UPDATE 语句
    struct UpdateStmt : Stmt {
        std::string table;
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> sets;
        std::unique_ptr<Expr> where;
    };

    // 新增：DROP 语句
    struct DropTableStmt : Stmt {
        std::string table;
        bool if_exists{ false };
    };

    using StmtPtr = std::unique_ptr<Stmt>;
    using ExprPtr = std::unique_ptr<Expr>;

} // namespace minidb