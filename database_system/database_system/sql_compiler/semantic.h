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
        // �����
        Status check_create(const CreateTableStmt*, std::vector<Quad>& q);
        Status check_insert(const InsertStmt*, std::vector<Quad>& q);
        Status check_select(const SelectStmt*, std::vector<Quad>& q);
        Status check_delete(const DeleteStmt*, std::vector<Quad>& q);
        Status check_update(const UpdateStmt*, std::vector<Quad>& q); // ����㱣������Ԫʽ��û�о�ȥ�� q ����

        // ���ʽ�����ƶ� + �д��ڼ�飨��Сд�����У�֧�� a.b��
        std::optional<DataType> expr_type(const Expr*, const TableDef&, Status&);

    private:
        ICatalog& cat_;
    };

} // namespace minidb

