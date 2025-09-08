// =============================================
// sql_compiler/parser.cpp
// =============================================
#include "parser.hpp"

namespace minidb {

    Token Parser::cur() { if (!has_) { t_ = lx_.next(); has_ = true; } return t_; }
    bool  Parser::is(TokenType t) { return cur().type == t; }
    bool  Parser::accept(TokenType t) { if (is(t)) { has_ = false; return true; } return false; }
    bool  Parser::accept_kw(const char* kw) { Token tk = cur(); if (tk.type == TokenType::KEYWORD && tk.lexeme == kw) { has_ = false; return true; } return false; }

    Token Parser::expect(TokenType t, Status& st, const char* msg) {
        Token tk = cur();
        if (tk.type != t) {
            st = Status::Error(std::string("Syntax error at ") + std::to_string(tk.line) + ":" + std::to_string(tk.col) + " expected " + msg);
        }
        else has_ = false;
        return tk;
    }

    StmtPtr Parser::parse_statement(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::KEYWORD) {
            if (tk.lexeme == "CREATE") return parse_create(st);
            if (tk.lexeme == "INSERT") return parse_insert(st);
            if (tk.lexeme == "SELECT") return parse_select(st);
            if (tk.lexeme == "DELETE") return parse_delete(st);
        }
        st = Status::Error("Unknown statement start");
        return nullptr;
    }

    StmtPtr Parser::parse_create(Status& st) {
        expect(TokenType::KEYWORD, st, "CREATE"); if (!st.ok) return nullptr;
        if (!accept_kw("TABLE")) { st = Status::Error("Expected TABLE"); return nullptr; }
        Token name = expect(TokenType::IDENT, st, "table name"); if (!st.ok) return nullptr;
        if (!accept(TokenType::LPAREN)) { st = Status::Error("Expected '('"); return nullptr; }

        TableDef td; td.name = name.lexeme;
        while (true) {
            Token cn = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
            Token ty = expect(TokenType::KEYWORD, st, "type");  if (!st.ok) return nullptr;
            DataType dt;
            if (ty.lexeme == "INT") dt = DataType::INT32;
            else if (ty.lexeme == "VARCHAR") dt = DataType::VARCHAR;
            else { st = Status::Error("Unsupported type"); return nullptr; }
            td.columns.push_back({ cn.lexeme, dt });

            if (accept(TokenType::COMMA)) continue;
            if (accept(TokenType::RPAREN)) break;
            st = Status::Error("Expected ',' or ')'");
            return nullptr;
        }
        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); return nullptr; }

        auto c = std::make_unique<CreateTableStmt>(); c->def = std::move(td);
        return c;
    }

    StmtPtr Parser::parse_insert(Status& st) {
        expect(TokenType::KEYWORD, st, "INSERT"); if (!st.ok) return nullptr;
        if (!accept_kw("INTO")) { st = Status::Error("Expected INTO"); return nullptr; }
        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) return nullptr;

        std::vector<std::string> cols;
        if (accept(TokenType::LPAREN)) {
            while (true) {
                Token cn = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
                cols.push_back(cn.lexeme);
                if (accept(TokenType::COMMA)) continue;
                if (accept(TokenType::RPAREN)) break;
                st = Status::Error("Expected ',' or ')'");
                return nullptr;
            }
        }

        if (!accept_kw("VALUES")) { st = Status::Error("Expected VALUES"); return nullptr; }
        if (!accept(TokenType::LPAREN)) { st = Status::Error("Expected '('"); return nullptr; }

        std::vector<std::unique_ptr<Expr>> vals;
        while (true) {
            auto e = parse_expr(st); if (!st.ok) return nullptr;
            vals.push_back(std::move(e));
            if (accept(TokenType::COMMA)) continue;
            if (accept(TokenType::RPAREN)) break;
            st = Status::Error("Expected ',' or ')'");
            return nullptr;
        }

        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); return nullptr; }

        auto x = std::make_unique<InsertStmt>();
        x->table = tn.lexeme; x->columns = std::move(cols); x->values = std::move(vals);
        return x;
    }

    StmtPtr Parser::parse_select(Status& st) {
        expect(TokenType::KEYWORD, st, "SELECT"); if (!st.ok) return nullptr;

        std::vector<std::string> cols; bool star = false;
        if (accept(TokenType::STAR)) star = true;
        else {
            while (true) {
                Token c = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
                cols.push_back(c.lexeme);
                if (accept(TokenType::COMMA)) continue;
                break;
            }
        }

        if (!accept_kw("FROM")) { st = Status::Error("Expected FROM"); return nullptr; }
        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) return nullptr;

        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) {
            where = parse_expr(st); if (!st.ok) return nullptr;
        }

        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); return nullptr; }

        auto s = std::make_unique<SelectStmt>();
        s->table = tn.lexeme; s->columns = std::move(cols); s->star = star; s->where = std::move(where);
        return s;
    }

    StmtPtr Parser::parse_delete(Status& st) {
        expect(TokenType::KEYWORD, st, "DELETE"); if (!st.ok) return nullptr;
        if (!accept_kw("FROM")) { st = Status::Error("Expected FROM"); return nullptr; }
        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) return nullptr;

        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) {
            where = parse_expr(st); if (!st.ok) return nullptr;
        }
        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); return nullptr; }

        auto d = std::make_unique<DeleteStmt>();
        d->table = tn.lexeme; d->where = std::move(where);
        return d;
    }

    std::unique_ptr<Expr> Parser::parse_primary(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::IDENT) { has_ = false; return std::make_unique<ColRef>(tk.lexeme); }
        if (tk.type == TokenType::INTCONST) { has_ = false; return std::make_unique<IntLit>(std::stoi(tk.lexeme)); }
        if (tk.type == TokenType::STRCONST) { has_ = false; return std::make_unique<StrLit>(tk.lexeme); }
        st = Status::Error("Expected primary expr");
        return nullptr;
    }

    std::unique_ptr<Expr> Parser::parse_expr(Status& st) {
        auto lhs = parse_primary(st); if (!st.ok) return nullptr;
        Token tk = cur();
        if (tk.type == TokenType::EQ || tk.type == TokenType::NEQ || tk.type == TokenType::LT ||
            tk.type == TokenType::LE || tk.type == TokenType::GT || tk.type == TokenType::GE) {
            has_ = false;
            auto rhs = parse_primary(st); if (!st.ok) return nullptr;
            CmpOp op = tk.type == TokenType::EQ ? CmpOp::EQ :
                tk.type == TokenType::NEQ ? CmpOp::NEQ :
                tk.type == TokenType::LT ? CmpOp::LT :
                tk.type == TokenType::LE ? CmpOp::LE :
                tk.type == TokenType::GT ? CmpOp::GT : CmpOp::GE;
            return std::make_unique<CmpExpr>(std::move(lhs), op, std::move(rhs));
        }
        return lhs; // 允许单一 primary（如 SELECT name FROM t;）
    }

} // namespace minidb