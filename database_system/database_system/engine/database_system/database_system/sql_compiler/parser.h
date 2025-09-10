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

        // 主入口：分析一条语句（以 ';' 结尾），失败时 st.ok=false
        StmtPtr parse_statement(Status& st);

        // 可选：启用/禁用“步骤跟踪”，用于展示 [step, rule, lookahead, action]
        void enable_trace(bool on) { trace_on_ = on; }
        const std::vector<std::string>& trace_log() const { return trace_; }
        void clear_trace() { trace_.clear(); }

    private:
        // 基本工具
        Token cur();                 // 查看当前 lookahead
        bool is(TokenType t);
        bool accept(TokenType t);    // 若匹配则前进
        bool accept_kw(const char* kw);
        Token expect(TokenType t, Status& st, const char* what);
        Token expect_kw(const char* kw, Status& st, const char* what);

        // 错误恢复：跳到 ';' 或 END（并尽量吃掉 ';'）
        void sync_to_semi();

        // 产生式（递归下降）
        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);

        // 表达式：cmp := primary ( (=|!=|<|<=|>|>=) primary )?
        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        // 跟踪日志
        void trace(const std::string& s);

    private:
        Lexer& lx_;
        Token t_{};
        bool has_{ false };

        bool trace_on_{ false };
        std::vector<std::string> trace_;
    };

} // namespace minidb