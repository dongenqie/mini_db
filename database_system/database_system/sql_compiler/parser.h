// =============================================
// sql_compiler/parser.h
// �ݹ��½��﷨���������Ľ��棩�����õĴ�����ʾ + ͬ���ָ� + ���̸���
// =============================================
// sql_compiler/parser.h  �������루��ȷ������������ǰ�棩
#ifndef MINIDB_PARSER_DEBUG
#define MINIDB_PARSER_DEBUG 0
#endif

#pragma once
#include "lexer.h"
#include "ast.h"
#include "../utils/common.h"
#include <vector>
#include <string>

namespace minidb {

    class Parser {
    public:
        // �£�Ĭ�ϸ��ݺ�����Ƿ� trace��Ĭ�� 0 -> �رգ�
        explicit Parser(Lexer& lx, bool enable_trace = (MINIDB_PARSER_DEBUG != 0))
            : lx_(lx), trace_on_(enable_trace) {}

        StmtPtr parse_statement(Status& st);

        void set_trace(bool on) { trace_on_ = on; }
        void enable_trace(bool on) { trace_on_ = on; }

        const std::vector<std::string>& trace_log() const { return trace_; }

    private:
        // �ʷ�ǰհ/ƥ��
        Token cur();
        bool is(TokenType t);
        bool accept(TokenType t);
        bool accept_kw(const char* kw);

        // ���� ͳһ��ǿ expect���ӿڣ�����ϸ���� + trace + ͬ���ָ� ���� 
        Token expect(TokenType t, Status& st, const char* what);
        Token expect_kw(const char* kw, Status& st, const char* what);
        void expect_or_sync(Status& st, const char* expected_msg);

        void sync_to_semi();

        // ���ʽ
        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        // ���
        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);
        // ���� ���� ���� 
        StmtPtr parse_update(Status& st);
        StmtPtr parse_drop(Status& st);

        // ���� SELECT ��չ���ӹ��� ���� 
        bool parse_qualified_name(std::string& out);
        bool parse_joins(Status& st, std::vector<SelectJoin>& joins);
        bool parse_group_by(Status& st, std::vector<std::string>& out);
        bool parse_order_by(Status& st, std::vector<OrderItem>& out);

        // ���� ������أ�����ʦʾ����ӡ�� ����
        void trace_push(const std::string& sym);          // ��ջ
        void trace_pop();                                 // ��ջ
        void trace_use_rule(int rule_no, const std::string& rule);
        void trace_match(const std::string& what);
        void trace_match_tok(const Token& matched, const std::string& what);
        void trace_accept();                              // ���ս���
        std::string snapshot_input_until_semi();
        void trace_print_header_once();

        // ��ѡ������������־���棨��ǰ��ֱ�Ӵ�ӡ��
        void trace(const std::string& s);

        void clear_trace();

    private:
        Lexer& lx_;
        Token t_{}; 
        bool has_{ false };

        bool trace_on_{ false };
        int step_{ 0 };
        bool printed_header_{ false };
        std::vector<std::string> stack_;
        std::vector<std::string> trace_;   // <== ֮ǰȱ������±������

    };

} // namespace minidb