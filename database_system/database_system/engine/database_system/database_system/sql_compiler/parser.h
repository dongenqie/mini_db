// =============================================
// sql_compiler/parser.h
// �ݹ��½��﷨���������Ľ��棩�����õĴ�����ʾ + ͬ���ָ� + ���̸���
// =============================================
#pragma once
#include "lexer.h"
#include "ast.h"
#include "../utils/common.h"
#include <vector>
#include <string>

namespace minidb {

    class Parser {
    public:
        explicit Parser(Lexer& lx) : lx_(lx) {}

        // ����ڣ�����һ����䣨�� ';' ��β����ʧ��ʱ st.ok=false
        StmtPtr parse_statement(Status& st);

        // ��ѡ������/���á�������١�������չʾ [step, rule, lookahead, action]
        void enable_trace(bool on) { trace_on_ = on; }
        const std::vector<std::string>& trace_log() const { return trace_; }
        void clear_trace() { trace_.clear(); }

    private:
        // ��������
        Token cur();                 // �鿴��ǰ lookahead
        bool is(TokenType t);
        bool accept(TokenType t);    // ��ƥ����ǰ��
        bool accept_kw(const char* kw);
        Token expect(TokenType t, Status& st, const char* what);
        Token expect_kw(const char* kw, Status& st, const char* what);

        // ����ָ������� ';' �� END���������Ե� ';'��
        void sync_to_semi();

        // ����ʽ���ݹ��½���
        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);

        // ���ʽ��cmp := primary ( (=|!=|<|<=|>|>=) primary )?
        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        // ������־
        void trace(const std::string& s);

    private:
        Lexer& lx_;
        Token t_{};
        bool has_{ false };

        bool trace_on_{ false };
        std::vector<std::string> trace_;
    };

} // namespace minidb