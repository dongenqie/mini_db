// =============================================
// sql_compiler/semantic.h
// =============================================
#pragma once
#include "ast.h"
#include "ir.h" 
#include "catalog_iface.h"   
#include <vector>
#include <unordered_map>

namespace minidb {

    struct SemanticResult { Status status; std::vector<Quad> quads; };

    class SemanticAnalyzer {
    public:
        explicit SemanticAnalyzer(ICatalog& c) : cat_(c) {}
        SemanticResult analyze(Stmt* s);

    private:
        // 各语句
        Status check_create(const CreateTableStmt*, std::vector<Quad>& q);
        Status check_insert(const InsertStmt*, std::vector<Quad>& q);
        Status check_select(const SelectStmt*, std::vector<Quad>& q);
        Status check_delete(const DeleteStmt*, std::vector<Quad>& q);
        Status check_update(const UpdateStmt*, std::vector<Quad>& q); // 如果你保留了四元式；没有就去掉 q 参数

        // 表达式类型推断 + 列存在检查（大小写不敏感，支持 a.b）
        std::optional<DataType> expr_type(const Expr*, const TableDef&, Status&);

    private:
        ICatalog& cat_;
    };

} // namespace minidb

