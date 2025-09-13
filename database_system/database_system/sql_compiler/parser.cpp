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

    void Parser::expect_or_sync(Status& st, const char* expected_msg) {
        Token tk = cur();
        std::ostringstream os;
        os << "Syntax error at " << tk.line << ":" << tk.col
            << " : expected " << expected_msg
            << ", got \"" << tk.lexeme << "\"";
        st = Status::Error(os.str());
        // 追踪一行错误（不强制，但便于排查）
        if (trace_on_) {
            ++step_;
            std::cout << "[" << step_ << "] "
                << snapshot_input_until_semi()
                << ", ERROR " << expected_msg
                << " but got \"" << tk.lexeme << "\"]\n";
        }
        sync_to_semi(); // 关键：同步到 ';'
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
        if (tk.lexeme == "UPDATE") return parse_update(st);   // <== 新增
        if (tk.lexeme == "DROP")   return parse_drop(st);

        st = Status::Error("Unknown statement at " + pos_str(tk) + " : " + tk.lexeme);
        sync_to_semi();
        return nullptr;
    }

    // ---------- CREATE ----------
    StmtPtr Parser::parse_create(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Create ';'\n"
                << "(2) Create -> CREATE TABLE ID '(' ColDefs ')'\n"
                << "(3) ColDefs -> ColDef (',' ColDef)*\n"
                << "(4) ColDef -> ID Type\n"
                << "(5) Type -> INT | VARCHAR\n";
        }

        trace_push("Prog");
        trace_use_rule(1, "Prog -> Create ';'");              // 展开 Prog
        trace_use_rule(2, "Create -> CREATE TABLE ID '(' ColDefs ')'");

        // CREATE
        Token kwCreate = expect_kw("CREATE", st, "CREATE"); if (!st.ok) return nullptr;
        trace_match_tok(kwCreate, "CREATE");

        // TABLE
        Token kwTable = expect_kw("TABLE", st, "TABLE"); if (!st.ok) return nullptr;
        trace_match_tok(kwTable, "TABLE");

        // 表名
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

                // 兼容两种长度写法：VARCHAR(64) 或 VARCHAR 64
                if (is(TokenType::LPAREN)) {
                    // VARCHAR(64)
                    Token lp = expect(TokenType::LPAREN, st, "'('");
                    if (!st.ok) return nullptr;
                    trace_match_tok(lp, "'('");

                    Token len = expect(TokenType::INTCONST, st, "varchar length");
                    if (!st.ok) return nullptr;
                    // 跟踪时把长度当作一个普通数字展示
                    trace_match_tok(len, len.lexeme);

                    Token rp = expect(TokenType::RPAREN, st, "')'");
                    if (!st.ok) return nullptr;
                    trace_match_tok(rp, "')'");
                    // 当前实现不使用长度值，可忽略 len.lexeme
                }
                else if (is(TokenType::INTCONST)) {
                    // VARCHAR 64
                    Token len = expect(TokenType::INTCONST, st, "varchar length");
                    if (!st.ok) return nullptr;
                    trace_match_tok(len, len.lexeme);
                    // 同样忽略具体长度
                }
                else {
                    // 不强制要求写长度（如仅写 VARCHAR），也可在此放宽
                    // 若要强制，请改为 expect_or_sync 报错：
                    // expect_or_sync(st, "varchar length like '(N)' or ' N'");
                    // return nullptr;
                }
            }
            else {
                st = Status::Error("Unsupported type: " + ty.lexeme);
                return nullptr;
            }

            td.columns.push_back({ cn.lexeme, dt });

            // ',' ColDef 继续
            if (is(TokenType::COMMA)) {
                Token comma = expect(TokenType::COMMA, st, "','"); if (!st.ok) return nullptr;
                trace_match_tok(comma, "','");
                // 回到 while 顶继续打一条 ColDef 规则
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
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Insert ';'\n"
                << "(2) Insert -> INSERT INTO Tbl OptCols VALUES '(' ValList ')'\n"
                << "(3) Tbl -> ID\n"
                << "(4) OptCols -> '(' ColList ')' | ε\n"
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

        // OptCols -> '(' ColList ')' | ε
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
            trace_use_rule(4, "OptCols -> ε");
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

        // 第一个 Expr
        {
            auto e = parse_expr(st); if (!st.ok) return nullptr;
            // 这里为了打印“匹配 expr”，做一个虚拟 token（仅用于展示）
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
            vals.push_back(std::move(e));
        }
        // 其后 , Expr*
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
        // 规则清单（保持原样）
        if (trace_on_) {
            std::cout
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Query ';'\n"
                << "(2) Query -> SELECT SelList FROM Tbl\n"
                << "(3) SelList -> ID\n"
                << "(4) Tbl -> ID\n";
        }

        // 构造 SelectStmt 对象（！！非常关键，以便后面 joins/group/order 能引用到）
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

            // 解析第一个选择项：支持 ID('.'ID)?
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

            // 后续：, ID('.'ID)? 重复
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

        // FROM 后可选**别名**（不存 AST，只吃掉以便后续 JOIN/ORDER 能正常识别）
        if (is(TokenType::IDENT)) {
            Token alias = expect(TokenType::IDENT, st, "alias");
            if (!st.ok) return nullptr;
            // 仅用于 trace 展示，避免改 AST
            trace_match_tok(alias, "ID(" + alias.lexeme + ")");
        }

        // WHERE（可选）
        std::unique_ptr<Expr> where;
        if (is(TokenType::KEYWORD) && cur().lexeme == "WHERE") {
            Token whereTk = expect(TokenType::KEYWORD, st, "WHERE");
            if (!st.ok) return nullptr;
            trace_match_tok(whereTk, "WHERE");

            where = parse_expr(st);
            if (!st.ok) return nullptr;

            // 仅用于输出一步“匹配 expr”，不改变词法游标
            Token fakeExpr{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(fakeExpr, "expr");
        }

        // ====== 这里是关键：把 s 的字段先落下，后续扩展都往 s 上填 ======
        s->table = tn.lexeme;
        s->columns = std::move(cols);
        s->star = star;
        s->where = std::move(where);

        // ====== 关键新增：SELECT 尾部扩展 ======
    // 1) JOIN 串
        if (!parse_joins(st, s->joins)) return nullptr;
        if (!st.ok) return nullptr;

        // 2) GROUP BY
        if (!parse_group_by(st, s->group_by)) return nullptr;
        if (!st.ok) return nullptr;

        // 2.5) HAVING（在你现有接口外补一句，极小改动）
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

        // 接受
        trace_accept();

        return s;
    }

    // ---------- DELETE ----------
    StmtPtr Parser::parse_delete(Status& st) {
        if (trace_on_) {
            std::cout
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Delete ';'\n"
                << "(2) Delete -> DELETE FROM Tbl OptWhere\n"
                << "(3) Tbl -> ID\n"
                << "(4) OptWhere -> WHERE Expr | ε\n";
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

        // OptWhere -> WHERE Expr | ε
        if (accept_kw("WHERE")) {
            // 手里没有 expect_kw 拿到的 token；补一个展示用 token:
            Token kwWhere{ TokenType::KEYWORD, "WHERE", tn.line, tn.col };
            trace_use_rule(4, "OptWhere -> WHERE Expr");
            trace_match_tok(kwWhere, "WHERE");
            where = parse_expr(st); if (!st.ok) return nullptr;
            Token exprTok{ TokenType::IDENT, "expr", cur().line, cur().col };
            trace_match_tok(exprTok, "expr");
        }
        else {
            trace_use_rule(4, "OptWhere -> ε");
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
                << " ============== 规则清单 ==============\n"
                << "(1) Prog -> Update ';'\n"
                << "(2) Update -> UPDATE Tbl SET AssignList OptWhere\n"
                << "(3) Tbl -> ID\n"
                << "(4) AssignList -> Assign (',' Assign)*\n"
                << "(5) Assign -> ID '=' Expr\n"
                << "(6) OptWhere -> WHERE Expr | ε\n";
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
            // 展示用
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
            trace_use_rule(6, "OptWhere -> ε");
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


    // —— JOIN 列表： (INNER|LEFT|RIGHT|FULL)? JOIN ID ON Expr 反复 —— 
    bool Parser::parse_joins(Status& st, std::vector<SelectJoin>& joins) {
        while (true) {
            // 预判是否出现 JOIN
            Token tk = cur();
            if (tk.type != TokenType::KEYWORD) break;

            JoinType jt = JoinType::INNER; // 默认 INNER
            if (accept_kw("INNER")) jt = JoinType::INNER;
            else if (accept_kw("LEFT"))  jt = JoinType::LEFT;
            else if (accept_kw("RIGHT")) jt = JoinType::RIGHT;
            else if (accept_kw("FULL"))  jt = JoinType::FULL;
            // else: 无限定词，视为直接 JOIN

            if (!accept_kw("JOIN")) {
                // 没 JOIN，回退前面错误？这里当作无 JOIN 结束
                //（因为前面的 INNER/LEFT/RIGHT/FULL 如果出现但后面没有 JOIN，会在 expect_kw("JOIN") 时报错）
                if (jt != JoinType::INNER) { expect_or_sync(st, "JOIN"); return false; }
                break;
            }
            Token fakeJoin{ TokenType::KEYWORD, "JOIN", tk.line, tk.col };
            trace_match_tok(fakeJoin, "JOIN");

            // 表名
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
            // 如果你的 SelectJoin 里有 alias 字段且你在上面读了别名，可选：sj.alias = alias;
            sj.on = std::move(e);
            joins.push_back(std::move(sj));
        }
        return true;
    }

    // —— GROUP BY col (, col)* (HAVING Expr)? —— 
    bool Parser::parse_group_by(Status& st, std::vector<std::string>& out) {
        if (!accept_kw("GROUP")) return true;    // 没有 GROUP -> 正常返回
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

    // —— ORDER BY col (ASC|DESC)? (, ...)* —— 
    bool Parser::parse_order_by(Status& st, std::vector<OrderItem>& out) {
        if (!accept_kw("ORDER")) return true;    // 没有 ORDER -> 正常返回
        Token kwO{ TokenType::KEYWORD, "ORDER", cur().line, cur().col };
        trace_match_tok(kwO, "ORDER");

        Token kwB = expect_kw("BY", st, "BY"); if (!st.ok) return false;
        trace_match_tok(kwB, "BY");

        // 第一项
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

    // ---------- 表达式 ----------
    // ==== 覆盖 parse_primary：支持 a.b ====
    std::unique_ptr<Expr> Parser::parse_primary(Status& st) {
        Token tk = cur();
        if (tk.type == TokenType::IDENT) {
            // 先吃掉第一个 ident
            has_ = false;
            std::string name = tk.lexeme;
            // 看是否跟着 "."
            if (cur().type == TokenType::DOT) {
                has_ = false; // 吃掉 DOT
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