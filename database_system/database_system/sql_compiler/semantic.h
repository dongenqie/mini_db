// =============================================
// sql_compiler/semantic.h
// =============================================
#pragma once
#include "ast.h"
#include "catalog_iface.h"   

namespace minidb {

    struct SemanticResult { Status status; };

    class SemanticAnalyzer {
    public:
        explicit SemanticAnalyzer(ICatalog& c) : cat_(c) {}
        SemanticResult analyze(Stmt* s);

    private:
        Status check_create(const CreateTableStmt*);
        Status check_insert(const InsertStmt*);
        Status check_select(const SelectStmt*);
        Status check_delete(const DeleteStmt*);

        std::optional<DataType> expr_type(const Expr*, const TableDef&, Status&);

    private:
        ICatalog& cat_;
    };

} // namespace minidb

