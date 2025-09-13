// =============================================
// sql_compiler/parser.cpp �﷨����
// =============================================
#define MINIDB_IMPL_PARSER
#include "parser.h"
#include <sstream>
#include <iostream>

namespace minidb {

    // ---------- С���� ----------
    static std::string pos_str(const Token& tk) {
        std::ostringstream os;
        os << tk.line << ":" << tk.col;
        return os.str();
    }

    void Parser::trace(const std::string& s) {
        if (trace_on_) trace_.push_back(s);
    }

    void Parser::clear_trace() {
        trace_.clear();
        stack_.clear();
        step_ = 0;
        printed_header_ = false;
    }

    bool Parser::parse_qualified_name(std::string& out) {
        Status dummy = Status::OK();
        Token first = cur();
        if (first.type != TokenType::IDENT) return false;
        accept(TokenType::IDENT);
        out = first.lexeme;

        if (is(TokenType::DOT)) {
            accept(TokenType::DOT);
            Token second = cur();
            if (second.type != TokenType::IDENT) return false;
            accept(TokenType::IDENT);
            out += ".";
            out += second.lexeme;
        }
        return true;
    }

    // ---------- �ʷ�ǰհ ----------
    Token Parser::cur() {
        if (!has_) { t_ = lx_.next(); has_ = true; }
        return t_;
    }
    bool Parser::is(TokenType t) { return cur().type == t; }

    // ��Щƥ�亯������**ֻ�ı��α�**�����ٴ�ӡ��־��ͳһ�� trace_match �������ɶ���־
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

    void Parser::expect_or_sync(Status& st, const char* expected_msg) {
        Token tk = cur();
        std::ostringstream os;
        os << "Syntax error at " << tk.line << ":" << tk.col
            << " : expected " << expected_msg
            << ", got \"" << tk.lexeme << "\"";
        st = Status::Error(os.str());
        // ׷��һ�д��󣨲�ǿ�ƣ��������Ų飩
        if (trace_on_) {
            ++step_;
            std::cout << "[" << step_ << "] "
                << snapshot_input_until_semi()
                << ", ERROR " << expected_msg
                << " but got \"" << tk.lexeme << "\"]\n";
        }
        sync_to_semi(); // �ؼ���ͬ���� ';'
    }

    // ============ ������� ============
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
    // ��������ʾջ�������ӡ�������� "';'" �Ѻû����Ϊ ";"
    static std::string join_vec_rev(const std::vector<std::string>& v) {
        auto fmt = [](const std::string& x) -> std::string {
            return (x == "';'") ? ";" : x;  // ��ȥ���ֺŵ����ţ���������
            };
        std::string s = "[";
        // ���������ջ������ջ�����ң����� PPT չʾ��
        for (size_t k = 0; k < v.size(); ++k) {
            const std::string& sym = v[v.size() - 1 - k];
            s += fmt(sym);
            if (k + 1 < v.size()) s += " ";
        }
        s += "]";
        return s;
    }
    std::string Parser::snapshot_input_until_semi() {
        // 1) �ȱ���ʷ���������״̬���������� lookahead��
        auto saved = lx_.save();

        auto fmt = [](const Token& t) -> std::string {
            if (t.type == TokenType::KEYWORD) return t.lexeme;
            if (t.type == TokenType::IDENT)   return std::string("ID:") + t.lexeme;
            return t.lexeme; // ���� ",", "(", ")", ";", ������
            };

        std::vector<std::string> parts;

        // 2) ��������ǰ���š�����Դ��ֻ������Ӱ����ʵ�α꣩
        //    - �� Parser ������ǰհ���棨has_==true������ǰ���ž��� t_��
        //      ��ʱ Lexer ���ڲ��α��Ѿ��� t_ ֮���ˣ�����ֱ�Ӵӵ�ǰλ�ü������Ӻ��� token��
        //    - �� Parser ��û�л��棨has_==false������ Lexer.peek() �ȿ�һ����Ϊ��ǰ���ţ�
        //      Ȼ����ʱ next() һ�°������Ե������������Ӻ��� token��
        bool used_parser_cache = has_;   // ��ס�����Ƿ�ʹ���� Parser �� t_ ��Ϊ��ǰ����
        if (used_parser_cache) {
            // ��ǰ���ž��� t_����Ҫ���� Parser::cur()��ֻ��ӡ
            parts.push_back(fmt(t_));
            // ע�⣺��ʱ lx_ ��״̬�Ѿ��� t_ ֮����Ϊ�������ù� Parser::cur() �� t_ ȡ�����ˣ�
            // ����ֱ�������Ӽ���
        }
        else {
            // û�� parser ���棬�ôʷ����� peek() ��һ��
            Token first = lx_.peek();          // ���ı�ʷ�����λ�ã����Լ��� la_��
            parts.push_back(fmt(first));
            (void)lx_.next();                  // ��ʱǰ��һ����֮��� restore ����
        }

        // 3) �ڡ���ʱ�ƽ����Ļ������������ ';'���� END��Ϊֹ
        while (true) {
            Token t = lx_.next();
            if (t.type == TokenType::END) break;
            parts.push_back(fmt(t));
            if (t.type == TokenType::SEMI) break;
        }

        // 4) ��ԭ�ʷ���������ǰ��״̬����Ӱ�� Parser �� has_/t_��
        lx_.restore(saved);

        // 5) ��װ��ʾ��
        std::string s = "(";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) s += " ";
            s += parts[i];
        }
        s += ")";
        return s;
    }

    // �� �������ѡ��ո�ƥ��� token����Ϊ���յĵ�һ��Ԫ����ʾ����
    void Parser::trace_match_tok(const Token& matched, const std::string& what) {
        if (!trace_on_) return;

        // 1) ����ʷ���״̬
        auto saved = lx_.save();

        auto fmt = [](const Token& t) -> std::string {
            if (t.type == TokenType::KEYWORD) return t.lexeme;
            if (t.type == TokenType::IDENT)   return std::string("ID:") + t.lexeme;
            return t.lexeme;
            };

        // 2) ���� parts���ȷš��ո�ƥ��� token������ƴ�ӡ��˿�ʣ�����롱���ӵ�ǰ�ʷ�λ�ÿ��ӣ�
        std::vector<std::string> parts;
        parts.push_back(fmt(matched));

        // ��ʱ�ƽ�����ֱ�� ';' �� END��ע�⣺ֻ�ôʷ�������� restore��
        while (true) {
            Token t = lx_.next();
            if (t.type == TokenType::END) break;
            parts.push_back(fmt(t));
            if (t.type == TokenType::SEMI) break;
        }

        lx_.restore(saved);

        // 3) ��װ�ַ���
        std::string snap = "(";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) snap += " ";
            snap += parts[i];
        }
        snap += ")";

        // 4) ��ӡһ��
        ++step_;
        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snap << ", ƥ�� " << what << "]\n";

        // 5) ջ����һ�������Ҳ����Ŷ�Ӧ��
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
            << snapshot_input_until_semi() << ", ��(" << rule_no << ") " << rule << "]\n";

        // �����󲿣�stack_ �����ǽ�Ҫչ���ķ��ս����
        if (!stack_.empty()) stack_.pop_back();

        // ���� "A -> B C ';'" ���Ҳ�������ѹ����ʾջ
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
            << snapshot_input_until_semi() << ", ƥ�� " << what << "]\n";
        // ����һ����ʾջԪ�أ�������������ջ���Ҳ����Ŷ�Ӧ��
        if (!stack_.empty()) stack_.pop_back();
    }
    void Parser::trace_accept() {
        if (!trace_on_) return;
        ++step_;
        std::cout << "[" << step_ << "] " << join_vec_rev(stack_) << ", "
            << snapshot_input_until_semi() << ", ����(Accept)]\n";
    }

    // ---------- ͬ���ָ� ----------
    void Parser::sync_to_semi() {
        while (true) {
            Token tk = cur();
            if (tk.type == TokenType::SEMI) { has_ = false; break; }
            if (tk.type == TokenType::END) { break; }
            has_ = false;
        }
    }

    // ---------- ���� ----------
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
        if (tk.lexeme == "UPDATE") return parse_update(st);   // <== ����
        if (tk.lexeme == "DROP")   return parse_drop(st);

        st = Status::Error("Unknown statement at " + pos_str(tk) + " : " + tk.lexeme);
        sync_to_semi();
        return nullptr;
    }

    // ---------- CREATE ----------
    StmtPtr Parser::parse_create(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== �����嵥 ==============\n"
                << "(1) Prog -> Create ';'\n"
                << "(2) Create -> CREATE TABLE ID '(' ColDefs ')'\n"
                << "(3) ColDefs -> ColDef (',' ColDef)*\n"
                << "(4) ColDef -> ID Type\n"
                << "(5) Type -> INT | VARCHAR\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Create ';'");              // չ�� Prog
        trace_use_rule(2, "Create -> CREATE TABLE ID '(' ColDefs ')'");

        // CREATE
        Token kwCreate = expect_kw("CREATE", st, "CREATE"); if (!st.ok) return nullptr;
        trace_match_tok(kwCreate, "CREATE");

        // TABLE
        Token kwTable = expect_kw("TABLE", st, "TABLE"); if (!st.ok) return nullptr;
        trace_match_tok(kwTable, "TABLE");

        // ����
        Token name = expect(TokenType::IDENT, st, "table name"); if (!st.ok) return nullptr;
        trace_match_tok(name, "ID(" + name.lexeme + ")");

        // '('
        Token lp = expect(TokenType::LPAREN, st, "'('"); if (!st.ok) return nullptr;
        trace_match_tok(lp, "'('");

        // ColDefs
        trace_use_rule(3, "ColDefs -> ColDef (',' ColDef)*");

        TableDef td; td.name = name.lexeme;

        // ColDef
        while (true) {
            trace_use_rule(4, "ColDef -> ID Type");
            Token cn = expect(TokenType::IDENT, st, "column name"); if (!st.ok) return nullptr;
            trace_match_tok(cn, "ID(" + cn.lexeme + ")");

            Token ty = expect(TokenType::KEYWORD, st, "type(INT/VARCHAR)");
            if (!st.ok) return nullptr;
            trace_match_tok(ty, ty.lexeme);

            DataType dt;
            if (ty.lexeme == "INT") {
                dt = DataType::INT32;
            }
            else if (ty.lexeme == "VARCHAR") {
                dt = DataType::VARCHAR;

                // �������ֳ���д����VARCHAR(64) �� VARCHAR 64
                if (is(TokenType::LPAREN)) {
                    // VARCHAR(64)
                    Token lp = expect(TokenType::LPAREN, st, "'('");
                    if (!st.ok) return nullptr;
                    trace_match_tok(lp, "'('");

                    Token len = expect(TokenType::INTCONST, st, "varchar length");
                    if (!st.ok) return nullptr;
                    // ����ʱ�ѳ��ȵ���һ����ͨ����չʾ
                    trace_match_tok(len, len.lexeme);

                    Token rp = expect(TokenType::RPAREN, st, "')'");
                    if (!st.ok) return nullptr;
                    trace_match_tok(rp, "')'");
                    // ��ǰʵ�ֲ�ʹ�ó���ֵ���ɺ��� len.lexeme
                }
                else if (is(TokenType::INTCONST)) {
                    // VARCHAR 64
                    Token len = expect(TokenType::INTCONST, st, "varchar length");
                    if (!st.ok) return nullptr;
                    trace_match_tok(len, len.lexeme);
                    // ͬ�����Ծ��峤��
                }
                else {
                    // ��ǿ��Ҫ��д���ȣ����д VARCHAR����Ҳ���ڴ˷ſ�
                    // ��Ҫǿ�ƣ����Ϊ expect_or_sync ����
                    // expect_or_sync(st, "varchar length like '(N)' or ' N'");
                    // return nullptr;
                }
            }
            else {
                st = Status::Error("Unsupported type: " + ty.lexeme);
                return nullptr;
            }

            td.columns.push_back({ cn.lexeme, dt });

            // ',' ColDef ����
            if (is(TokenType::COMMA)) {
                Token comma = expect(TokenType::COMMA, st, "','"); if (!st.ok) return nullptr;
                trace_match_tok(comma, "','");
                // �ص� while ��������һ�� ColDef ����
                continue;
            }
            break;
        }

        // ')'
        Token rp = expect(TokenType::RPAREN, st, "')'"); if (!st.ok) return nullptr;
        trace_match_tok(rp, "')'");

        // ';'
        Token semi = expect(TokenType::SEMI, st, "';'"); if (!st.ok) return nullptr;
        trace_match_tok(semi, "';'");
        trace_accept();

        auto c = std::make_unique<CreateTableStmt>();
        c->def = std::move(td);
        return c;
    }

    // ---------- INSERT ----------
    StmtPtr Parser::parse_insert(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== �����嵥 ==============\n"
                << "(1) Prog -> Insert ';'\n"
                << "(2) Insert -> INSERT INTO Tbl OptCols VALUES '(' ValList ')'\n"
                << "(3) Tbl -> ID\n"
                << "(4) OptCols -> '(' ColList ')' | ��\n"
                << "(5) ColList -> ID (',' ID)*\n"
                << "(6) ValList -> Expr (',' Expr)*\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Insert ';'");
        trace_use_rule(2, "Insert -> INSERT INTO Tbl OptCols VALUES '(' ValList ')'");

        // INSERT
        Token kwIns = expect_kw("INSERT", st, "INSERT"); if (!st.ok) return nullptr;
        trace_match_tok(kwIns, "INSERT");

        // INTO
        Token kwInto = expect_kw("INTO", st, "INTO"); if (!st.ok) return nullptr;
        trace_match_tok(kwInto, "INTO");

        // Tbl -> ID
        trace_use_rule(3, "Tbl -> ID");
        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        std::vector<std::string> cols;

        // OptCols -> '(' ColList ')' | ��
        if (is(TokenType::LPAREN)) {
            trace_use_rule(4, "OptCols -> '(' ColList ')'");
            Token lp = expect(TokenType::LPAREN, st, "'('"); if (!st.ok) return nullptr;
            trace_match_tok(lp, "'('");

            // ColList -> ID (',' ID)*
            trace_use_rule(5, "ColList -> ID (',' ID)*");
            Token c1 = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
            cols.push_back(c1.lexeme);
            trace_match_tok(c1, "ID(" + c1.lexeme + ")");

            while (is(TokenType::COMMA)) {
                Token comma = expect(TokenType::COMMA, st, "','"); if (!st.ok) return nullptr;
                trace_match_tok(comma, "','");
                Token ci = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
                cols.push_back(ci.lexeme);
                trace_match_tok(ci, "ID(" + ci.lexeme + ")");
            }

            Token rp = expect(TokenType::RPAREN, st, "')'"); if (!st.ok) return nullptr;
            trace_match_tok(rp, "')'");
        }
        else {
            trace_use_rule(4, "OptCols -> ��");
        }

        // VALUES
        Token kwVals = expect_kw("VALUES", st, "VALUES"); if (!st.ok) return nullptr;
        trace_match_tok(kwVals, "VALUES");

        // '('
        Token lp2 = expect(TokenType::LPAREN, st, "'('"); if (!st.ok) return nullptr;
        trace_match_tok(lp2, "'('");

        // ValList -> Expr (',' Expr)*
        trace_use_rule(6, "ValList -> Expr (',' Expr)*");

        std::vector<std::unique_ptr<Expr>> vals;

        // ��һ�� Expr
        {
            auto e = parse_expr(st); if (!st.ok) return nullptr;
            // ����Ϊ�˴�ӡ��ƥ�� expr������һ������ token��������չʾ��
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
            vals.push_back(std::move(e));
        }
        // ��� , Expr*
        while (is(TokenType::COMMA)) {
            Token comma = expect(TokenType::COMMA, st, "','"); if (!st.ok) return nullptr;
            trace_match_tok(comma, "','");
            auto e = parse_expr(st); if (!st.ok) return nullptr;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
            vals.push_back(std::move(e));
        }

        // ')'
        Token rp2 = expect(TokenType::RPAREN, st, "')'"); if (!st.ok) return nullptr;
        trace_match_tok(rp2, "')'");

        // ';'
        Token semi = expect(TokenType::SEMI, st, "';'"); if (!st.ok) return nullptr;
        trace_match_tok(semi, "';'");
        trace_accept();

        auto x = std::make_unique<InsertStmt>();
        x->table = tn.lexeme; x->columns = std::move(cols); x->values = std::move(vals);
        return x;
    }

    // ---------- SELECT ----------
    StmtPtr Parser::parse_select(Status& st) {
        // �����嵥������ԭ����
        if (trace_on_) {
            std::cout
                << " ============== �����嵥 ==============\n"
                << "(1) Prog -> Query ';'\n"
                << "(2) Query -> SELECT SelList FROM Tbl\n"
                << "(3) SelList -> ID\n"
                << "(4) Tbl -> ID\n";
        }

        // ���� SelectStmt ���󣨣����ǳ��ؼ����Ա���� joins/group/order �����õ���
        auto s = std::make_unique<SelectStmt>();

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Query ';'");
        trace_use_rule(2, "Query -> SELECT SelList FROM Tbl");

        // SELECT
        Token sel = expect(TokenType::KEYWORD, st, "SELECT");
        if (!st.ok) return nullptr;
        trace_match_tok(sel, "SELECT");

        // SelList
        std::vector<std::string> cols; bool star = false;

        if (is(TokenType::STAR)) {
            trace_use_rule(3, "SelList -> *");
            Token starTk = expect(TokenType::STAR, st, "*");
            if (!st.ok) return nullptr;
            trace_match_tok(starTk, "*");
            star = true;
        }
        else {
            trace_use_rule(3, "SelList -> ID");

            // ������һ��ѡ���֧�� ID('.'ID)?
            {
                Token c1 = expect(TokenType::IDENT, st, "column");
                if (!st.ok) return nullptr;

                std::string item = c1.lexeme;
                trace_match_tok(c1, "ID(" + c1.lexeme + ")");

                if (is(TokenType::DOT)) {
                    Token dot = expect(TokenType::DOT, st, "'.'");
                    if (!st.ok) return nullptr;
                    trace_match_tok(dot, "'.'");

                    Token c2 = expect(TokenType::IDENT, st, "column");
                    if (!st.ok) return nullptr;
                    trace_match_tok(c2, "ID(" + c2.lexeme + ")");

                    item += ".";
                    item += c2.lexeme;
                }
                cols.push_back(item);
            }

            // ������, ID('.'ID)? �ظ�
            while (is(TokenType::COMMA)) {
                Token comma = expect(TokenType::COMMA, st, "','");
                if (!st.ok) return nullptr;
                trace_match_tok(comma, "','");

                Token c = expect(TokenType::IDENT, st, "column");
                if (!st.ok) return nullptr;

                std::string item = c.lexeme;
                trace_match_tok(c, "ID(" + c.lexeme + ")");

                if (is(TokenType::DOT)) {
                    Token dot = expect(TokenType::DOT, st, "'.'");
                    if (!st.ok) return nullptr;
                    trace_match_tok(dot, "'.'");

                    Token c2 = expect(TokenType::IDENT, st, "column");
                    if (!st.ok) return nullptr;
                    trace_match_tok(c2, "ID(" + c2.lexeme + ")");

                    item += ".";
                    item += c2.lexeme;
                }
                cols.push_back(item);
            }
        }

        // FROM
        {
            Token fromTk = expect_kw("FROM", st, "FROM");
            if (!st.ok) return nullptr;
            trace_match_tok(fromTk, "FROM");
        }

        // Tbl -> ID
        trace_use_rule(4, "Tbl -> ID");
        Token tn = expect(TokenType::IDENT, st, "table");
        if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        // FROM ���ѡ**����**������ AST��ֻ�Ե��Ա���� JOIN/ORDER ������ʶ��
        if (is(TokenType::IDENT)) {
            Token alias = expect(TokenType::IDENT, st, "alias");
            if (!st.ok) return nullptr;
            // ������ trace չʾ������� AST
            trace_match_tok(alias, "ID(" + alias.lexeme + ")");
        }

        // WHERE����ѡ��
        std::unique_ptr<Expr> where;
        if (is(TokenType::KEYWORD) && cur().lexeme == "WHERE") {
            Token whereTk = expect(TokenType::KEYWORD, st, "WHERE");
            if (!st.ok) return nullptr;
            trace_match_tok(whereTk, "WHERE");

            where = parse_expr(st);
            if (!st.ok) return nullptr;

            // ���������һ����ƥ�� expr�������ı�ʷ��α�
            Token fakeExpr{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(fakeExpr, "expr");
        }

        // ====== �����ǹؼ����� s ���ֶ������£�������չ���� s ���� ======
        s->table = tn.lexeme;
        s->columns = std::move(cols);
        s->star = star;
        s->where = std::move(where);

        // ====== �ؼ�������SELECT β����չ ======
    // 1) JOIN ��
        if (!parse_joins(st, s->joins)) return nullptr;
        if (!st.ok) return nullptr;

        // 2) GROUP BY
        if (!parse_group_by(st, s->group_by)) return nullptr;
        if (!st.ok) return nullptr;

        // 2.5) HAVING���������нӿ��ⲹһ�䣬��С�Ķ���
        if (accept_kw("HAVING")) {
            Token kwHaving{ TokenType::KEYWORD, "HAVING", tn.line, tn.col };
            trace_match_tok(kwHaving, "HAVING");
            s->having = parse_expr(st);
            if (!st.ok) return nullptr;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
        }

        // 3) ORDER BY
        if (!parse_order_by(st, s->order_by)) return nullptr;
        if (!st.ok) return nullptr;

        // ';'
        {
            Token semi = expect(TokenType::SEMI, st, "';'");
            if (!st.ok) return nullptr;
            trace_match_tok(semi, "';'");
        }

        // ����
        trace_accept();

        return s;
    }

    // ---------- DELETE ----------
    StmtPtr Parser::parse_delete(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== �����嵥 ==============\n"
                << "(1) Prog -> Delete ';'\n"
                << "(2) Delete -> DELETE FROM Tbl OptWhere\n"
                << "(3) Tbl -> ID\n"
                << "(4) OptWhere -> WHERE Expr | ��\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Delete ';'");
        trace_use_rule(2, "Delete -> DELETE FROM Tbl OptWhere");

        // DELETE
        Token kwDel = expect_kw("DELETE", st, "DELETE"); if (!st.ok) return nullptr;
        trace_match_tok(kwDel, "DELETE");

        // FROM
        Token kwFrom = expect_kw("FROM", st, "FROM"); if (!st.ok) return nullptr;
        trace_match_tok(kwFrom, "FROM");

        // Tbl -> ID
        trace_use_rule(3, "Tbl -> ID");
        Token tn = expect(TokenType::IDENT, st, "table name"); if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        std::unique_ptr<Expr> where;

        // OptWhere -> WHERE Expr | ��
        if (accept_kw("WHERE")) {
            // ����û�� expect_kw �õ��� token����һ��չʾ�� token:
            Token kwWhere{ TokenType::KEYWORD, "WHERE", tn.line, tn.col };
            trace_use_rule(4, "OptWhere -> WHERE Expr");
            trace_match_tok(kwWhere, "WHERE");
            where = parse_expr(st); if (!st.ok) return nullptr;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
        }
        else {
            trace_use_rule(4, "OptWhere -> ��");
        }

        // ';'
        Token semi = expect(TokenType::SEMI, st, "';'"); if (!st.ok) return nullptr;
        trace_match_tok(semi, "';'");
        trace_accept();

        auto d = std::make_unique<DeleteStmt>();
        d->table = tn.lexeme; d->where = std::move(where);
        return d;
    }

    // ============== UPDATE ==============
    StmtPtr Parser::parse_update(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== �����嵥 ==============\n"
                << "(1) Prog -> Update ';'\n"
                << "(2) Update -> UPDATE Tbl SET AssignList OptWhere\n"
                << "(3) Tbl -> ID\n"
                << "(4) AssignList -> Assign (',' Assign)*\n"
                << "(5) Assign -> ID '=' Expr\n"
                << "(6) OptWhere -> WHERE Expr | ��\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Update ';'");
        trace_use_rule(2, "Update -> UPDATE Tbl SET AssignList OptWhere");

        // UPDATE
        Token kwUp = expect_kw("UPDATE", st, "UPDATE"); if (!st.ok) return nullptr;
        trace_match_tok(kwUp, "UPDATE");

        // Tbl -> ID
        trace_use_rule(3, "Tbl -> ID");
        Token tn = expect(TokenType::IDENT, st, "table"); if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        // SET
        Token kwSet = expect_kw("SET", st, "SET"); if (!st.ok) return nullptr;
        trace_match_tok(kwSet, "SET");

        // AssignList
        trace_use_rule(4, "AssignList -> Assign (',' Assign)*");

        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> sets;

        // Assign -> ID '=' Expr
        while (true) {
            trace_use_rule(5, "Assign -> ID '=' Expr");
            Token col = expect(TokenType::IDENT, st, "column"); if (!st.ok) return nullptr;
            trace_match_tok(col, "ID(" + col.lexeme + ")");

            Token eq = expect(TokenType::EQ, st, "'='"); if (!st.ok) return nullptr;
            trace_match_tok(eq, "'='");

            auto e = parse_expr(st); if (!st.ok) return nullptr;
            // չʾ��
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");

            sets.push_back({ col.lexeme, std::move(e) });

            if (accept(TokenType::COMMA)) {
                Token comma{ TokenType::COMMA, ",", eq.line, eq.col };
                trace_match_tok(comma, "','");
                continue;
            }
            break;
        }

        // OptWhere
        std::unique_ptr<Expr> where;
        if (accept_kw("WHERE")) {
            Token fake{ TokenType::KEYWORD, "WHERE", tn.line, tn.col };
            trace_use_rule(6, "OptWhere -> WHERE Expr");
            trace_match_tok(fake, "WHERE");
            where = parse_expr(st); if (!st.ok) return nullptr;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
        }
        else {
            trace_use_rule(6, "OptWhere -> ��");
        }

        // ';'
        if (!accept(TokenType::SEMI)) { expect_or_sync(st, "';'"); return nullptr; }
        Token semi{ TokenType::SEMI, ";", cur().line, cur().col };
        trace_match_tok(semi, "';'");
        trace_accept();

        auto u = std::make_unique<UpdateStmt>();
        u->table = tn.lexeme;
        u->sets = std::move(sets);
        u->where = std::move(where);
        return u;
    }

    // =============== DROP ===============
    StmtPtr Parser::parse_drop(Status& st) {
        Token kwDrop = expect_kw("DROP", st, "DROP"); if (!st.ok) return nullptr;
        trace_match_tok(kwDrop, "DROP");

        Token kwTable = expect_kw("TABLE", st, "TABLE"); if (!st.ok) return nullptr;
        trace_match_tok(kwTable, "TABLE");

        bool if_exists = false;
        if (accept_kw("IF")) {
            Token fakeIf{ TokenType::KEYWORD, "IF", kwTable.line, kwTable.col };
            trace_match_tok(fakeIf, "IF");
            Token kwExists = expect_kw("EXISTS", st, "EXISTS"); if (!st.ok) return nullptr;
            trace_match_tok(kwExists, "EXISTS");
            if_exists = true;
        }

        Token tn = expect(TokenType::IDENT, st, "table name"); if (!st.ok) return nullptr;
        trace_match_tok(tn, "ID(" + tn.lexeme + ")");

        Token semi = expect(TokenType::SEMI, st, "';'"); if (!st.ok) return nullptr;
        trace_match_tok(semi, "';'");
        trace_accept();

        auto d = std::make_unique<DropTableStmt>();
        d->table = tn.lexeme;
        d->if_exists = if_exists;
        return d;
    }


    // ���� JOIN �б� (INNER|LEFT|RIGHT|FULL)? JOIN ID ON Expr ���� ���� 
    bool Parser::parse_joins(Status& st, std::vector<SelectJoin>& joins) {
        while (true) {
            // Ԥ���Ƿ���� JOIN
            Token tk = cur();
            if (tk.type != TokenType::KEYWORD) break;

            JoinType jt = JoinType::INNER; // Ĭ�� INNER
            if (accept_kw("INNER")) jt = JoinType::INNER;
            else if (accept_kw("LEFT"))  jt = JoinType::LEFT;
            else if (accept_kw("RIGHT")) jt = JoinType::RIGHT;
            else if (accept_kw("FULL"))  jt = JoinType::FULL;
            // else: ���޶��ʣ���Ϊֱ�� JOIN

            if (!accept_kw("JOIN")) {
                // û JOIN������ǰ��������ﵱ���� JOIN ����
                //����Ϊǰ��� INNER/LEFT/RIGHT/FULL ������ֵ�����û�� JOIN������ expect_kw("JOIN") ʱ����
                if (jt != JoinType::INNER) { expect_or_sync(st, "JOIN"); return false; }
                break;
            }
            Token fakeJoin{ TokenType::KEYWORD, "JOIN", tk.line, tk.col };
            trace_match_tok(fakeJoin, "JOIN");

            // ����
            Token tb = expect(TokenType::IDENT, st, "table"); if (!st.ok) return false;
            trace_match_tok(tb, "ID(" + tb.lexeme + ")");

            // ON
            Token onkw = expect_kw("ON", st, "ON"); if (!st.ok) return false;
            trace_match_tok(onkw, "ON");

            auto e = parse_expr(st); if (!st.ok) return false;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");

            SelectJoin sj;
            sj.type = jt;
            sj.table = tb.lexeme;
            // ������ SelectJoin ���� alias �ֶ�������������˱�������ѡ��sj.alias = alias;
            sj.on = std::move(e);
            joins.push_back(std::move(sj));
        }
        return true;
    }

    // ���� GROUP BY col (, col)* (HAVING Expr)? ���� 
    bool Parser::parse_group_by(Status& st, std::vector<std::string>& out) {
        if (!accept_kw("GROUP")) return true;    // û�� GROUP -> ��������
        Token kwG{ TokenType::KEYWORD, "GROUP", cur().line, cur().col };
        trace_match_tok(kwG, "GROUP");

        Token kwB = expect_kw("BY", st, "BY"); if (!st.ok) return false;
        trace_match_tok(kwB, "BY");

        Token c1 = expect(TokenType::IDENT, st, "group-by column"); if (!st.ok) return false;
        out.push_back(c1.lexeme);
        trace_match_tok(c1, "ID(" + c1.lexeme + ")");

        while (accept(TokenType::COMMA)) {
            Token comma{ TokenType::COMMA, ",", c1.line, c1.col };
            trace_match_tok(comma, "','");
            Token ci = expect(TokenType::IDENT, st, "group-by column"); if (!st.ok) return false;
            out.push_back(ci.lexeme);
            trace_match_tok(ci, "ID(" + ci.lexeme + ")");
        }
        return true;
    }

    // ���� ORDER BY col (ASC|DESC)? (, ...)* ���� 
    bool Parser::parse_order_by(Status& st, std::vector<OrderItem>& out) {
        if (!accept_kw("ORDER")) return true;    // û�� ORDER -> ��������
        Token kwO{ TokenType::KEYWORD, "ORDER", cur().line, cur().col };
        trace_match_tok(kwO, "ORDER");

        Token kwB = expect_kw("BY", st, "BY"); if (!st.ok) return false;
        trace_match_tok(kwB, "BY");

        // ��һ��
        Token c1 = expect(TokenType::IDENT, st, "order-by column"); if (!st.ok) return false;
        trace_match_tok(c1, "ID(" + c1.lexeme + ")");
        bool asc = true;
        if (accept_kw("ASC")) { Token t{ TokenType::KEYWORD,"ASC",c1.line,c1.col }; trace_match_tok(t, "ASC"); asc = true; }
        else if (accept_kw("DESC")) { Token t{ TokenType::KEYWORD,"DESC",c1.line,c1.col }; trace_match_tok(t, "DESC"); asc = false; }
        out.push_back(OrderItem{ c1.lexeme, asc });

        while (accept(TokenType::COMMA)) {
            Token comma{ TokenType::COMMA, ",", c1.line, c1.col };
            trace_match_tok(comma, "','");

            Token ci = expect(TokenType::IDENT, st, "order-by column"); if (!st.ok) return false;
            trace_match_tok(ci, "ID(" + ci.lexeme + ")");
            bool asc2 = true;
            if (accept_kw("ASC")) { Token t{ TokenType::KEYWORD,"ASC",ci.line,ci.col }; trace_match_tok(t, "ASC"); asc2 = true; }
            else if (accept_kw("DESC")) { Token t{ TokenType::KEYWORD,"DESC",ci.line,ci.col }; trace_match_tok(t, "DESC"); asc2 = false; }
            out.push_back(OrderItem{ ci.lexeme, asc2 });
        }
        return true;
    }

    // ---------- ���ʽ ----------
    // ==== ���� parse_primary��֧�� a.b ====
    std::unique_ptr<Expr> Parser::parse_primary(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::IDENT) {
            // �ȳԵ���һ�� ident
            has_ = false;
            std::string name = tk.lexeme;
            // ���Ƿ���� "."
            if (cur().type == TokenType::DOT) {
                has_ = false; // �Ե� DOT
                Token id2 = expect(TokenType::IDENT, st, "identifier"); if (!st.ok) return nullptr;
                name += ".";
                name += id2.lexeme;
            }
            return std::make_unique<ColRef>(name);
        }
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