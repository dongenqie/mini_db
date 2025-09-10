// =============================================
// sql_compiler/parser.cpp
// =============================================
#define MINIDB_IMPL_PARSER
#include "parser.h"
#include <sstream>

namespace minidb {

    // ---------- 小工具 ----------
    static std::string pos_str(const Token& tk) {
        std::ostringstream os;
        os << tk.line << ":" << tk.col;
        return os.str();
    }

    void Parser::trace(const std::string& s) {
        if (trace_on_) trace_.push_back(s);
    }

    // ---------- 词法前瞻 ----------
    Token Parser::cur() {
        if (!has_) { t_ = lx_.next(); has_ = true; }
        return t_;
    }
    bool Parser::is(TokenType t) { return cur().type == t; }
    bool Parser::accept(TokenType t) {
        // 先看看当前前瞻
        Token tk = cur();
        if (tk.type == t) {
            // 命中则消耗并可选记录一条跟踪日志
            has_ = false;
            trace(std::string("match TOK(") + std::to_string(static_cast<int>(t)) +
                ") @" + std::to_string(tk.line) + ":" + std::to_string(tk.col));
            return true;
        }
        return false;
    }
    bool Parser::accept_kw(const char* kw) {
        Token tk = cur();
        if (tk.type == TokenType::KEYWORD && tk.lexeme == kw) {
            has_ = false;
            trace(std::string("match KW(") + kw + ") @" + pos_str(tk));
            return true;
        }
        return false;
    }

    Token Parser::expect(TokenType t, Status& st, const char* what) {
        Token tk = cur();
        if (tk.type != t) {
            std::ostringstream os;
            os << "Syntax error at " << pos_str(tk)
                << " : expected " << what
                << ", got \"" << tk.lexeme << "\"";
            st = Status::Error(os.str());
            trace(std::string("ERROR expected ") + what + " got \"" + tk.lexeme + "\"");
            return tk; // 不前进
        }
        has_ = false;
        trace(std::string("match TOK(") + what + ") @" + pos_str(tk));
        return tk;
    }

    Token Parser::expect_kw(const char* kw, Status& st, const char* what) {
        Token tk = cur();
        if (!(tk.type == TokenType::KEYWORD && tk.lexeme == kw)) {
            std::ostringstream os;
            os << "Syntax error at " << pos_str(tk)
                << " : expected " << what << " (KW " << kw << ")"
                << ", got \"" << tk.lexeme << "\"";
            st = Status::Error(os.str());
            trace(std::string("ERROR expected KW(") + kw + ") got \"" + tk.lexeme + "\"");
            return tk;
        }
        has_ = false;
        trace(std::string("match KW(") + kw + ") @" + pos_str(tk));
        return tk;
    }

    // ---------- 同步恢复 ----------
    void Parser::sync_to_semi() {
        // 将输入丢弃到 ';' 或 END
        while (true) {
            Token tk = cur();
            if (tk.type == TokenType::SEMI) { has_ = false; trace("sync eat ';'"); break; }
            if (tk.type == TokenType::END) { break; }
            has_ = false; // 前进
        }
    }

    // ---------- 顶层 ----------
    StmtPtr Parser::parse_statement(Status& st) {
        clear_trace();
        Token tk = cur();
        if (tk.type != TokenType::KEYWORD) {
            st = Status::Error("Syntax error at " + pos_str(tk) + " : expected statement keyword");
            sync_to_semi();
            return nullptr;
        }

        if (tk.lexeme == "CREATE") return parse_create(st);
        if (tk.lexeme == "INSERT") return parse_insert(st);
        if (tk.lexeme == "SELECT") return parse_select(st);
        if (tk.lexeme == "DELETE") return parse_delete(st);

        st = Status::Error("Unknown statement at " + pos_str(tk) + " : " + tk.lexeme);
        sync_to_semi();
        return nullptr;
    }

    // ---------- CREATE ----------
    StmtPtr Parser::parse_create(Status& st) {
        trace("enter CreateTable");
        expect_kw("CREATE", st, "CREATE");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        expect_kw("TABLE", st, "TABLE");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        Token name = expect(TokenType::IDENT, st, "table name");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        if (!accept(TokenType::LPAREN)) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected '(' after table name");
            sync_to_semi(); return nullptr;
        }

        TableDef td; td.name = name.lexeme;

        while (true) {
            Token cn = expect(TokenType::IDENT, st, "column name");
            if (!st.ok) { sync_to_semi(); return nullptr; }

            Token ty = expect(TokenType::KEYWORD, st, "type (INT/VARCHAR)");
            if (!st.ok) { sync_to_semi(); return nullptr; }

            DataType dt;
            if (ty.lexeme == "INT") dt = DataType::INT32;
            else if (ty.lexeme == "VARCHAR") dt = DataType::VARCHAR;
            else {
                st = Status::Error("Unsupported type at " + pos_str(ty) + " : " + ty.lexeme);
                sync_to_semi(); return nullptr;
            }
            td.columns.push_back({ cn.lexeme, dt });

            if (accept(TokenType::COMMA)) { trace("CreateTable ,"); continue; }
            if (accept(TokenType::RPAREN)) break;

            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected ',' or ')'");
            sync_to_semi(); return nullptr;
        }

        if (!accept(TokenType::SEMI)) {
            st = Status::Error("Missing ';' at end of CREATE TABLE");
            sync_to_semi(); return nullptr;
        }

        auto c = std::make_unique<CreateTableStmt>();
        c->def = std::move(td);
        trace("leave CreateTable (OK)");
        return c;
    }

    // ---------- INSERT ----------
    StmtPtr Parser::parse_insert(Status& st) {
        trace("enter Insert");
        expect_kw("INSERT", st, "INSERT");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        if (!accept_kw("INTO")) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected INTO");
            sync_to_semi(); return nullptr;
        }

        Token tn = expect(TokenType::IDENT, st, "table name");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        std::vector<std::string> cols;
        if (accept(TokenType::LPAREN)) {
            while (true) {
                Token cn = expect(TokenType::IDENT, st, "column name");
                if (!st.ok) { sync_to_semi(); return nullptr; }
                cols.push_back(cn.lexeme);
                if (accept(TokenType::COMMA)) continue;
                if (accept(TokenType::RPAREN)) break;
                st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected ',' or ')'");
                sync_to_semi(); return nullptr;
            }
        }

        if (!accept_kw("VALUES")) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected VALUES");
            sync_to_semi(); return nullptr;
        }

        if (!accept(TokenType::LPAREN)) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected '(' after VALUES");
            sync_to_semi(); return nullptr;
        }

        std::vector<std::unique_ptr<Expr>> vals;
        while (true) {
            auto e = parse_expr(st);
            if (!st.ok) { sync_to_semi(); return nullptr; }
            vals.push_back(std::move(e));
            if (accept(TokenType::COMMA)) continue;
            if (accept(TokenType::RPAREN)) break;
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected ',' or ')'");
            sync_to_semi(); return nullptr;
        }

        if (!accept(TokenType::SEMI)) {
            st = Status::Error("Missing ';' at end of INSERT");
            sync_to_semi(); return nullptr;
        }

        auto x = std::make_unique<InsertStmt>();
        x->table = tn.lexeme;
        x->columns = std::move(cols);
        x->values = std::move(vals);
        trace("leave Insert (OK)");
        return x;
    }

    // ---------- SELECT ----------
    StmtPtr Parser::parse_select(Status& st) {
        trace("enter Select");
        expect_kw("SELECT", st, "SELECT");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        std::vector<std::string> cols;
        bool star = false;

        if (accept(TokenType::STAR)) {
            star = true;
        }
        else {
            while (true) {
                Token c = expect(TokenType::IDENT, st, "column");
                if (!st.ok) { sync_to_semi(); return nullptr; }
                cols.push_back(c.lexeme);
                if (accept(TokenType::COMMA)) continue;
                break;
            }
        }

        if (!accept_kw("FROM")) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected FROM");
            sync_to_semi(); return nullptr;
        }

        Token tn = expect(TokenType::IDENT, st, "table name");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) {
            where = parse_expr(st);
            if (!st.ok) { sync_to_semi(); return nullptr; }
        }

        if (!accept(TokenType::SEMI)) {
            st = Status::Error("Missing ';' at end of SELECT");
            sync_to_semi(); return nullptr;
        }

        auto s = std::make_unique<SelectStmt>();
        s->table = tn.lexeme;
        s->columns = std::move(cols);
        s->star = star;
        s->where = std::move(where);
        trace("leave Select (OK)");
        return s;
    }

    // ---------- DELETE ----------
    StmtPtr Parser::parse_delete(Status& st) {
        trace("enter Delete");
        expect_kw("DELETE", st, "DELETE");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        if (!accept_kw("FROM")) {
            st = Status::Error("Syntax error at " + pos_str(cur()) + " : expected FROM");
            sync_to_semi(); return nullptr;
        }

        Token tn = expect(TokenType::IDENT, st, "table name");
        if (!st.ok) { sync_to_semi(); return nullptr; }

        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) {
            where = parse_expr(st);
            if (!st.ok) { sync_to_semi(); return nullptr; }
        }

        if (!accept(TokenType::SEMI)) {
            st = Status::Error("Missing ';' at end of DELETE");
            sync_to_semi(); return nullptr;
        }

        auto d = std::make_unique<DeleteStmt>();
        d->table = tn.lexeme;
        d->where = std::move(where);
        trace("leave Delete (OK)");
        return d;
    }

    // ---------- 表达式 ----------
    std::unique_ptr<Expr> Parser::parse_primary(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::IDENT) {
            has_ = false;
            trace(std::string("primary ColRef '") + tk.lexeme + "' @" + pos_str(tk));
            return std::make_unique<ColRef>(tk.lexeme);
        }
        if (tk.type == TokenType::INTCONST) {
            has_ = false;
            trace(std::string("primary Int ") + tk.lexeme + " @" + pos_str(tk));
            return std::make_unique<IntLit>(std::stoi(tk.lexeme));
        }
        if (tk.type == TokenType::STRCONST) {
            has_ = false;
            trace(std::string("primary Str \"") + tk.lexeme + "\" @" + pos_str(tk));
            return std::make_unique<StrLit>(tk.lexeme);
        }

        st = Status::Error("Syntax error at " + pos_str(tk) + " : expected identifier/int/string");
        return nullptr;
    }

    std::unique_ptr<Expr> Parser::parse_expr(Status& st) {
        auto lhs = parse_primary(st);
        if (!st.ok) return nullptr;

        Token tk = cur();
        if (tk.type == TokenType::EQ || tk.type == TokenType::NEQ ||
            tk.type == TokenType::LT || tk.type == TokenType::LE ||
            tk.type == TokenType::GT || tk.type == TokenType::GE) {
            has_ = false;
            CmpOp op =
                tk.type == TokenType::EQ ? CmpOp::EQ :
                tk.type == TokenType::NEQ ? CmpOp::NEQ :
                tk.type == TokenType::LT ? CmpOp::LT :
                tk.type == TokenType::LE ? CmpOp::LE :
                tk.type == TokenType::GT ? CmpOp::GT : CmpOp::GE;

            auto rhs = parse_primary(st);
            if (!st.ok) return nullptr;

            trace("cmp-expr build @" + pos_str(tk));
            return std::make_unique<CmpExpr>(std::move(lhs), op, std::move(rhs));
        }
        return lhs;
    }

} // namespace minidb