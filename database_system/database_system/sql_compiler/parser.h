// =============================================
// sql_compiler/parser.h
// 递归下降语法分析器（改进版）：更好的错误提示 + 同步恢复 + 过程跟踪
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

        StmtPtr parse_statement(Status& st);

        void set_trace(bool on) { trace_on_ = on; }
        void enable_trace(bool on) { trace_on_ = on; }

        const std::vector<std::string>& trace_log() const { return trace_; }
        void clear_trace() { trace_.clear(); }

    private:
        // 词法前瞻/匹配
        Token cur();
        bool is(TokenType t);
        bool accept(TokenType t);
        bool accept_kw(const char* kw);
        Token expect(TokenType t, Status& st, const char* what);
        Token expect_kw(const char* kw, Status& st, const char* what);

        void sync_to_semi();

        // 表达式
        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        // 语句
        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);

        // —— 跟踪相关（按老师示例打印） ——
        void trace_push(const std::string& sym);          // 入栈
        void trace_pop();                                 // 出栈
        void trace_use_rule(int rule_no, const std::string& rule);
        void trace_match(const std::string& what);
        void trace_match_tok(const Token& matched, const std::string& what); // ★ 新增
        void trace_accept();                              // 最终接收
        std::string snapshot_input_until_semi();
        void trace_print_header_once();

        // 可选：保留文字日志缓存（当前不直接打印）
        void trace(const std::string& s);

    private:
        Lexer& lx_;
        Token t_{}; bool has_{ false };

        bool trace_on_{ true };
        int step_{ 0 };
        std::vector<std::string> stack_;
        bool printed_header_{ false };
        std::vector<std::string> trace_;   // <== 之前缺这个导致编译错误
    };

} // namespace minidb