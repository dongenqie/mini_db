// =============================================
// sql_compiler/semantic.h
// =============================================
#pragma once
#include "ast.hpp"
#include "../engine/catalog_manager.hpp"

namespace minidb {

    struct SemanticResult { Status status; };

    class SemanticAnalyzer {
    public:
        explicit SemanticAnalyzer(CatalogManager& c) : cat_(c) {}
        SemanticResult analyze(Stmt* s);

    private:
        Status check_create(const CreateTableStmt*);
        Status check_insert(const InsertStmt*);
        Status check_select(const SelectStmt*);
        Status check_delete(const DeleteStmt*);

        std::optional<DataType> expr_type(const Expr*, const TableDef&, Status&);

    private:
        CatalogManager& cat_;
    };

} // namespace minidb#pragma once
