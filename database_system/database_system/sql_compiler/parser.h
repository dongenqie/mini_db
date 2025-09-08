// =============================================
// sql_compiler/parser.hpp
// =============================================
#pragma once
#include "lexer.hpp"
#include "ast.hpp"

namespace minidb {

    class Parser {
    public:
        explicit Parser(Lexer& lx) : lx_(lx) {}
        StmtPtr parse_statement(Status& st);

    private:
        Token cur();
        bool is(TokenType t);
        bool accept(TokenType t);
        bool accept_kw(const char* kw);
        Token expect(TokenType t, Status& st, const char* msg);

        std::unique_ptr<Expr> parse_expr(Status& st);
        std::unique_ptr<Expr> parse_primary(Status& st);

        StmtPtr parse_create(Status& st);
        StmtPtr parse_insert(Status& st);
        StmtPtr parse_select(Status& st);
        StmtPtr parse_delete(Status& st);

    private:
        Lexer& lx_;
        Token t_{};
        bool has_{ false };
    };

} // namespace minidb
