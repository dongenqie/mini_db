// =============================================
// sql_compiler/parser.cpp 语法分析
// =============================================
#define MINIDB_IMPL_PARSER
#include "parser.h"
#include <sstream>
#include <iostream>

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

    // 这些匹配函数现在**只改变游标**，不再打印日志；统一由 trace_match 输出人类可读日志
    bool Parser::accept(TokenType t) {
        Token tk = cur();
        if (tk.type == t) { has_ = false; return true; }
        return false;
    }
    bool Parser::accept_kw(const char* kw) {
        Token tk = cur();
        if (tk.type == TokenType::KEYWORD && tk.lexeme == kw) { has_ = false; return true; }
        return false;
    }
    Token Parser::expect(TokenType t, Status& st, const char* what) {
        Token tk = cur();
        if (tk.type != t) {
            std::ostringstream os; os << "Syntax error at " << pos_str(tk)
                << " : expected " << what << ", got \"" << tk.lexeme << "\"";
            st = Status::Error(os.str());
            return tk;
        }
        has_ = false;
        return tk;
    }
    Token Parser::expect_kw(const char* kw, Status& st, const char* what) {
        Token tk = cur();
        if (!(tk.type == TokenType::KEYWORD && tk.lexeme == kw)) {
            std::ostringstream os; os << "Syntax error at " << pos_str(tk)
                << " : expected " << what << " (KW " << kw << ")"
                << ", got \"" << tk.lexeme << "\"";
            st = Status::Error(os.str());
            return tk;
        }
        has_ = false;
        return tk;
    }

    // ============ 跟踪输出 ============
    void Parser::trace_print_header_once() {
        if (!trace_on_ || printed_header_) return;
        printed_header_ = true;
        std::cout << "SYNTAX TRACE:\n";
    }
    static std::string join_vec(const std::vector<std::string>& v) {
        std::string s = "[";
        for (size_t i = 0; i < v.size(); ++i) { s += v[i]; if (i + 1 < v.size()) s += " "; }
        s += "]";
        return s;
    }
    // 新增：显示栈“反向打印”，并把 "';'" 友好化输出为 ";"
    static std::string join_vec_rev(const std::vector<std::string>& v) {
        auto fmt = [](const std::string& x) -> std::string {
            return (x == "';'") ? ";" : x;  // 仅去掉分号的引号，其他保持
            };
        std::string s = "[";
        // 反向输出：栈底在左，栈顶在右（符合 PPT 展示）
        for (size_t k = 0; k < v.size(); ++k) {
            const std::string& sym = v[v.size() - 1 - k];
            s += fmt(sym);
            if (k + 1 < v.size()) s += " ";
        }
        s += "]";
        return s;
    }
    std::string Parser::snapshot_input_until_semi() {
        // 1) 先保存词法器的完整状态（含其自身 lookahead）
        auto saved = lx_.save();

        auto fmt = [](const Token& t) -> std::string {
            if (t.type == TokenType::KEYWORD) return t.lexeme;
            if (t.type == TokenType::IDENT)   return std::string("ID:") + t.lexeme;
            return t.lexeme; // 包含 ",", "(", ")", ";", 常量等
            };

        std::vector<std::string> parts;

        // 2) 决定“当前符号”的来源（只读，不影响真实游标）
        //    - 若 Parser 里已有前瞻缓存（has_==true），当前符号就是 t_；
        //      此时 Lexer 的内部游标已经在 t_ 之后了，所以直接从当前位置继续窥视后续 token。
        //    - 若 Parser 里没有缓存（has_==false），用 Lexer.peek() 先看一眼作为当前符号，
        //      然后临时 next() 一下把它“吃掉”，继续窥视后续 token。
        bool used_parser_cache = has_;   // 记住我们是否使用了 Parser 的 t_ 作为当前符号
        if (used_parser_cache) {
            // 当前符号就是 t_，不要调用 Parser::cur()；只打印
            parts.push_back(fmt(t_));
            // 注意：此时 lx_ 的状态已经在 t_ 之后（因为当初调用过 Parser::cur() 把 t_ 取出来了）
            // 所以直接向后窥视即可
        }
        else {
            // 没有 parser 缓存，用词法器的 peek() 看一眼
            Token first = lx_.peek();          // 不改变词法器的位置（有自己的 la_）
            parts.push_back(fmt(first));
            (void)lx_.next();                  // 临时前进一步，之后会 restore 回来
        }

        // 3) 在“临时推进”的环境里，继续看到 ';'（或 END）为止
        while (true) {
            Token t = lx_.next();
            if (t.type == TokenType::END) break;
            parts.push_back(fmt(t));
            if (t.type == TokenType::SEMI) break;
        }

        // 4) 还原词法器到窥视前的状态（不影响 Parser 的 has_/t_）
        lx_.restore(saved);

        // 5) 组装显示串
        std::string s = "(";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) s += " ";
            s += parts[i];
        }
        s += ")";
        return s;
    }

    // ★ 新增：把“刚刚匹配的 token”作为快照的第一个元素显示出来
    void Parser::trace_match_tok(const Token& matched, const std::string& what) {
        if (!trace_on_) return;

        // 1) 保存词法器状态
        auto saved = lx_.save();

        auto fmt = [](const Token& t) -> std::string {
            if (t.type == TokenType::KEYWORD) return t.lexeme;
            if (t.type == TokenType::IDENT)   return std::string("ID:") + t.lexeme;
            return t.lexeme;
            };

        // 2) 构造 parts：先放“刚刚匹配的 token”，再拼接“此刻剩余输入”（从当前词法位置窥视）
        std::vector<std::string> parts;
        parts.push_back(fmt(matched));

        // 临时推进窥视直到 ';' 或 END（注意：只用词法器，最后 restore）
        while (true) {
            Token t = lx_.next();
            if (t.type == TokenType::END) break;
            parts.push_back(fmt(t));
            if (t.type == TokenType::SEMI) break;
        }

        lx_.restore(saved);

        // 3) 组装字符串
        std::string snap = "(";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) snap += " ";
            snap += parts[i];
        }
        snap += ")";

        // 4) 打印一步
        ++step_;
        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snap << ", 匹配 " << what << "]\n";

        // 5) 栈弹出一个（与右部符号对应）
        if (!stack_.empty()) stack_.pop_back();
    }

    void Parser::trace_push(const std::string& sym) {
        if (!trace_on_) return; trace_print_header_once(); stack_.push_back(sym);
    }
    void Parser::trace_pop() { if (!trace_on_) return; if (!stack_.empty()) stack_.pop_back(); }
    void Parser::trace_use_rule(int rule_no, const std::string& rule) {
        if (!trace_on_) return;
        trace_print_header_once();
        ++step_;

        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snapshot_input_until_semi() << ", 用(" << rule_no << ") " << rule << "]\n";

        // 弹出左部（stack_ 顶就是将要展开的非终结符）
        if (!stack_.empty()) stack_.pop_back();

        // 解析 "A -> B C ';'" 的右部，逆序压入显示栈
        auto p = rule.find("->");
        std::string rhs = (p == std::string::npos) ? "" : trim(rule.substr(p + 2));
        std::istringstream iss(rhs);
        std::vector<std::string> syms; std::string tok;
        while (iss >> tok) syms.push_back(tok);
        for (auto it = syms.rbegin(); it != syms.rend(); ++it) {
            stack_.push_back(*it);
        }
    }
    void Parser::trace_match(const std::string& what) {
        if (!trace_on_) return;
        ++step_;
        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snapshot_input_until_semi() << ", 匹配 " << what << "]\n";
        // 弹出一个显示栈元素（与我们上面入栈的右部符号对应）
        if (!stack_.empty()) stack_.pop_back();
    }
    void Parser::trace_accept() {
        if (!trace_on_) return;
        ++step_;
        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snapshot_input_until_semi() << ", 接受(Accept)]\n";
    }

    // ---------- 同步恢复 ----------
    void Parser::sync_to_semi() {
        while (true) {
            Token tk = cur();
            if (tk.type == TokenType::SEMI) { has_ = false; break; }
            if (tk.type == TokenType::END) { break; }
            has_ = false;
        }
    }

    // ---------- 顶层 ----------
    StmtPtr Parser::parse_statement(Status& st) {
        clear_trace();
        printed_header_ = false;
        step_ = 0;
        stack_.clear();

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
        trace_push("Prog");
        trace_use_rule(1, "Prog -> Create ';'");
        trace_push("Create");
        trace_use_rule(2, "Create -> CREATE TABLE TblDef");

        expect_kw("CREATE", st, "CREATE"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("CREATE");
        expect_kw("TABLE", st, "TABLE");  if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("TABLE");

        Token name = expect(TokenType::IDENT, st, "table name"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("ID(" + name.lexeme + ")");

        if (!accept(TokenType::LPAREN)) { st = Status::Error("Expected '('"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("(");

        TableDef td; td.name = name.lexeme;
        while (true) {
            Token cn = expect(TokenType::IDENT, st, "column"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
            trace_match("ID(" + cn.lexeme + ")");
            Token ty = expect(TokenType::KEYWORD, st, "type"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
            trace_match(ty.lexeme);

            DataType dt;
            if (ty.lexeme == "INT") dt = DataType::INT32;
            else if (ty.lexeme == "VARCHAR") dt = DataType::VARCHAR;
            else { st = Status::Error("Unsupported type: " + ty.lexeme); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
            td.columns.push_back({ cn.lexeme, dt });

            if (accept(TokenType::COMMA)) { trace_match(","); continue; }
            if (accept(TokenType::RPAREN)) { trace_match(")"); break; }
            st = Status::Error("Expected ',' or ')'"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr;
        }
        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("';'");
        trace_accept();

        trace_pop(); trace_pop();
        auto c = std::make_unique<CreateTableStmt>(); c->def = std::move(td); return c;
    }

    // ---------- INSERT ----------
    StmtPtr Parser::parse_insert(Status& st) {
        trace_push("Prog");    trace_use_rule(1, "Prog -> Insert ';'");
        trace_push("Insert");  trace_use_rule(2, "Insert -> INSERT INTO T (ColList)? VALUES (ValList)");

        expect_kw("INSERT", st, "INSERT"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("INSERT");

        if (!accept_kw("INTO")) { st = Status::Error("Expected INTO"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("INTO");

        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("ID(" + tn.lexeme + ")");

        std::vector<std::string> cols;
        if (accept(TokenType::LPAREN)) {
            trace_match("(");
            while (true) {
                Token cn = expect(TokenType::IDENT, st, "column"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
                cols.push_back(cn.lexeme); trace_match("ID(" + cn.lexeme + ")");
                if (accept(TokenType::COMMA)) { trace_match(","); continue; }
                if (accept(TokenType::RPAREN)) { trace_match(")"); break; }
                st = Status::Error("Expected ',' or ')'"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr;
            }
        }

        if (!accept_kw("VALUES")) { st = Status::Error("Expected VALUES"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("VALUES");

        if (!accept(TokenType::LPAREN)) { st = Status::Error("Expected '(' after VALUES"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("(");

        std::vector<std::unique_ptr<Expr>> vals;
        while (true) {
            auto e = parse_expr(st); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
            vals.push_back(std::move(e)); trace_match("expr");
            if (accept(TokenType::COMMA)) { trace_match(","); continue; }
            if (accept(TokenType::RPAREN)) { trace_match(")"); break; }
            st = Status::Error("Expected ',' or ')'"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr;
        }

        if (!accept(TokenType::SEMI)) { st = Status::Error("Missing ';'"); trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("';'");
        trace_accept();

        trace_pop(); trace_pop();
        auto x = std::make_unique<InsertStmt>(); x->table = tn.lexeme; x->columns = std::move(cols); x->values = std::move(vals); return x;
    }

    // ---------- SELECT ----------
    StmtPtr Parser::parse_select(Status& st) {
        // 规则清单（保持原样）
        if (trace_on_) {
            std::cout
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Query ';'\n"
                << "(2) Query -> SELECT SelList FROM Tbl\n"
                << "(3) SelList -> ID\n"
                << "(4) Tbl -> ID\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Query ';'");
        trace_use_rule(2, "Query -> SELECT SelList FROM Tbl");

        // SELECT
        Token sel = expect(TokenType::KEYWORD, st, "SELECT");
        if (!st.ok) return nullptr;
        trace_match_tok(sel, "SELECT");

        // SelList
        std::vector<std::string> cols; bool star = false;
        // 只按老师规则：SelList -> ID（保留 * 的兼容可自行扩展）
        trace_use_rule(3, "SelList -> ID");
        Token c = expect(TokenType::IDENT, st, "column");
        if (!st.ok) return nullptr;
        cols.push_back(c.lexeme);
        trace_match_tok(c, "ID(" + c.lexeme + ")");

        // 允许额外的 , ID …（这部分非规则清单的扩展）
        while (accept(TokenType::COMMA)) {
            Token commaTok{ TokenType::COMMA, ",", c.line, c.col };
            trace_match_tok(commaTok, ",");
            Token c2 = expect(TokenType::IDENT, st, "column");
            if (!st.ok) return nullptr;
            cols.push_back(c2.lexeme);
            trace_match_tok(c2, "ID(" + c2.lexeme + ")");
        }

        // FROM
        Token kwFrom = expect_kw("FROM", st, "FROM");
        if (!st.ok) return nullptr;
        trace_match_tok(kwFrom, "FROM");

        // Tbl -> ID
        trace_use_rule(4, "Tbl -> ID");
        Token tn = expect(TokenType::IDENT, st, "table");
        if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        // 可选 WHERE（如要规范打印，也可仿照上面做 expect/trace_match_tok）
        if (accept_kw("WHERE")) {
            Token kwWhere{ TokenType::KEYWORD, "WHERE", tn.line, tn.col };
            trace_match_tok(kwWhere, "WHERE");
            auto where = parse_expr(st);
            if (!st.ok) return nullptr;
            // 这里为了简洁，表达式内部不逐 token 打“匹配”轨迹
        }

        // ';'
        Token semi = expect(TokenType::SEMI, st, "';'");
        if (!st.ok) return nullptr;
        trace_match_tok(semi, "';'");
        trace_accept();

        auto s = std::make_unique<SelectStmt>();
        s->table = tn.lexeme; s->columns = std::move(cols); s->star = star;
        return s;
    }

    // ---------- DELETE ----------
    StmtPtr Parser::parse_delete(Status& st) {
        trace_push("Prog");  trace_use_rule(1, "Prog -> Delete ';'");
        trace_push("Delete"); trace_use_rule(2, "Delete -> DELETE FROM Tbl (WHERE Expr)?");

        expect_kw("DELETE", st, "DELETE"); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("DELETE");

        if (!accept_kw("FROM")) {
            st = Status::Error("Expected FROM");
            trace_pop(); trace_pop(); sync_to_semi(); return nullptr;
        }
        trace_match("FROM");

        Token tn = expect(TokenType::IDENT, st, "table name");
        if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; }
        trace_match("ID(" + tn.lexeme + ")");

        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) { trace_match("WHERE"); where = parse_expr(st); if (!st.ok) { trace_pop(); trace_pop(); sync_to_semi(); return nullptr; } }

        if (!accept(TokenType::SEMI)) {
            st = Status::Error("Missing ';' at end of DELETE");
            trace_pop(); trace_pop(); sync_to_semi(); return nullptr;
        }
        trace_match("';'");

        trace_pop(); trace_pop();
        auto d = std::make_unique<DeleteStmt>(); d->table = tn.lexeme; d->where = std::move(where); return d;
    }

    // ---------- 表达式 ----------
    std::unique_ptr<Expr> Parser::parse_primary(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::IDENT) { has_ = false; trace(std::string("primary ColRef '") + tk.lexeme + "' @" + pos_str(tk)); return std::make_unique<ColRef>(tk.lexeme); }
        if (tk.type == TokenType::INTCONST) { has_ = false; trace(std::string("primary Int ") + tk.lexeme + " @" + pos_str(tk));     return std::make_unique<IntLit>(std::stoi(tk.lexeme)); }
        if (tk.type == TokenType::STRCONST) { has_ = false; trace(std::string("primary Str \"") + tk.lexeme + "\" @" + pos_str(tk)); return std::make_unique<StrLit>(tk.lexeme); }
        st = Status::Error("Syntax error at " + pos_str(tk) + " : expected identifier/int/string");
        return nullptr;
    }

    std::unique_ptr<Expr> Parser::parse_expr(Status& st) {
        auto lhs = parse_primary(st); if (!st.ok) return nullptr;
        Token tk = cur();
        if (tk.type == TokenType::EQ || tk.type == TokenType::NEQ || tk.type == TokenType::LT ||
            tk.type == TokenType::LE || tk.type == TokenType::GT || tk.type == TokenType::GE) {
            has_ = false;
            CmpOp op = tk.type == TokenType::EQ ? CmpOp::EQ :
                tk.type == TokenType::NEQ ? CmpOp::NEQ :
                tk.type == TokenType::LT ? CmpOp::LT :
                tk.type == TokenType::LE ? CmpOp::LE :
                tk.type == TokenType::GT ? CmpOp::GT : CmpOp::GE;
            auto rhs = parse_primary(st); if (!st.ok) return nullptr;
            trace("cmp-expr build @" + pos_str(tk));
            return std::make_unique<CmpExpr>(std::move(lhs), op, std::move(rhs));
        }
        return lhs;
    }

} // namespace minidb