// =============================================
// sql_compiler/parser.h
// 递归下降语法分析器（改进版）：更好的错误提示 + 同步恢复 + 过程跟踪
// =============================================
// sql_compiler/parser.h  顶部加入（或确保存在且在最前面）
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
        // 新：默认根据宏决定是否开 trace（默认 0 -> 关闭）
        explicit Parser(Lexer& lx, bool enable_trace = (MINIDB_PARSER_DEBUG != 0))
            : lx_(lx), trace_on_(enable_trace) {}

        StmtPtr parse_statement(Status& st);

        void set_trace(bool on) { trace_on_ = on; }
        void enable_trace(bool on) { trace_on_ = on; }

        const std::vector<std::string>& trace_log() const { return trace_; }

    private:
        // 词法前瞻/匹配
        Token cur();
        bool is(TokenType t);
        bool accept(TokenType t);
        bool accept_kw(const char* kw);

        // —— 统一“强 expect”接口：带详细错误 + trace + 同步恢复 —— 
        Token expect(TokenType t, Status& st, const char* what);
        Token expect_kw(const char* kw, Status& st, const char* what);
        void expect_or_sync(Status& st, const char* expected_msg);

        void sync_to_semi();

        // 表达式
        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        // 语句
        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);
        // —— 新增 —— 
        StmtPtr parse_update(Status& st);
        StmtPtr parse_drop(Status& st);

        // —— SELECT 扩展的子过程 —— 
        bool parse_qualified_name(std::string& out);
        bool parse_joins(Status& st, std::vector<SelectJoin>& joins);
        bool parse_group_by(Status& st, std::vector<std::string>& out);
        bool parse_order_by(Status& st, std::vector<OrderItem>& out);

        // —— 跟踪相关（按老师示例打印） ——
        void trace_push(const std::string& sym);          // 入栈
        void trace_pop();                                 // 出栈
        void trace_use_rule(int rule_no, const std::string& rule);
        void trace_match(const std::string& what);
        void trace_match_tok(const Token& matched, const std::string& what);
        void trace_accept();                              // 最终接收
        std::string snapshot_input_until_semi();
        void trace_print_header_once();

        // 可选：保留文字日志缓存（当前不直接打印）
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
        std::vector<std::string> trace_;   // <== 之前缺这个导致编译错误

    };

} // namespace minidb