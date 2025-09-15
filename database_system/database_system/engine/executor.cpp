// =============================================
// engine/executor.cpp
// =============================================
#include "executor.hpp"
#include "../sql_compiler/planner.h"
#include "../sql_compiler/ast.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <filesystem>

// 统一使用“引擎”的列/类型，避免与 minidb::Column 冲突
using EngColumn = ::Column;
using EngColumnType = ::ColumnType;

static inline std::string ltrim1(std::string s) { while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin()); return s; }
static inline std::string rtrim1(std::string s) { while (!s.empty() && std::isspace((unsigned char)s.back()))  s.pop_back(); return s; }
static inline std::string trim1(std::string s) { return rtrim1(ltrim1(std::move(s))); }
static inline std::string upper1(std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; }

// 按逗号切分 “( ... )” 的内容，忽略引号/括号内的逗号
static std::vector<std::string> split_items_respecting_paren(const std::string& inside) {
    std::vector<std::string> out; std::string cur; int par = 0; char q = 0;
    for (char c : inside) {
        if (q) { if (c == q) q = 0; cur.push_back(c); continue; }
        if (c == '\'' || c == '"') { q = c; cur.push_back(c); continue; }
        if (c == '(') { ++par; cur.push_back(c); continue; }
        if (c == ')' && par > 0) { --par; cur.push_back(c); continue; }
        if (c == ',' && par == 0) { out.push_back(trim1(cur)); cur.clear(); continue; }
        cur.push_back(c);
    }
    if (!trim1(cur).empty()) out.push_back(trim1(cur));
    return out;
}


static std::string data_path_for(const std::string& fileName) {
    std::filesystem::create_directories("data");
    return std::string("data/") + fileName;
}

static inline std::string trim_copy_local(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

static std::string trim_semicolon(std::string s) {
    // 去两端空白
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back()))  s.pop_back();
    // 去尾部分号
    if (!s.empty() && s.back() == ';') s.pop_back();
    // 再去一次尾空白（防御）
    while (!s.empty() && isspace((unsigned char)s.back()))  s.pop_back();
    return s;
}

static std::string up(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

// 解析列类型与属性：INT(10) [UNSIGNED] / TINYINT[(M)] / CHAR(n) / VARCHAR(n) / DECIMAL(p,s) / TIMESTAMP / FLOAT
// 支持 INT(10) / DECIMAL(10,2) 这种把括号与类型黏在同一 token 的写法
static bool parse_type_and_attrs(std::istringstream& is, EngColumn& col) {
    auto UP = [](std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; };

    // 从 token 中直接解析 (...) 的两个整数
    auto parse_from_token = [](const std::string& tok, int& a, int& b, bool two, bool& ok) {
        ok = false;
        auto lp = tok.find('(');
        auto rp = tok.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos || rp <= lp) return;
        std::string inside = tok.substr(lp + 1, rp - lp - 1);
        if (!two) {
            try { a = std::stoi(inside); ok = true; }
            catch (...) { ok = false; }
        }
        else {
            auto comma = inside.find(',');
            if (comma == std::string::npos) return;
            try {
                a = std::stoi(inside.substr(0, comma));
                // 跳过可选空格
                size_t p = comma + 1;
                while (p < inside.size() && isspace((unsigned char)inside[p])) ++p;
                b = std::stoi(inside.substr(p));
                ok = true;
            }
            catch (...) { ok = false; }
        }
        };

    // 从流里读取 '(' 后的一或两个整数
    auto parse_from_stream = [&](int& a, int& b, bool two)->bool {
        if (is.peek() != '(') return false;
        char ch; is >> ch;           // '('
        is >> a;
        if (two) {
            char c; is >> c; if (c != ',') return false;
            // 跳过可选空格
            while (isspace(is.peek())) is.get();
            is >> b;
        }
        is >> ch; // ')'
        return true;
        };

    std::string t;
    if (!(is >> t)) return false;
    std::string T = UP(t);

    // INT / INT(10)
    if (T.rfind("INT", 0) == 0) {
        col.type = EngColumnType::INT; col.length = 0;
        int m = 0, dummy = 0; bool ok = false;
        parse_from_token(T, m, dummy, /*two*/false, ok);
        if (ok) col.length = m;
        else {
            std::streampos pos = is.tellg();
            if (parse_from_stream(m, dummy, false)) col.length = m;
            else { is.clear(); is.seekg(pos); }
        }
    }
    // TINYINT / TINYINT(3)
    else if (T.rfind("TINYINT", 0) == 0) {
        col.type = EngColumnType::TINYINT; col.length = 0;
        int m = 0, dummy = 0; bool ok = false;
        parse_from_token(T, m, dummy, /*two*/false, ok);
        if (ok) col.length = m;
        else {
            std::streampos pos = is.tellg();
            if (parse_from_stream(m, dummy, false)) col.length = m;
            else { is.clear(); is.seekg(pos); }
        }
    }
    // CHAR(n)
    else if (T.rfind("CHAR", 0) == 0) {
        col.type = EngColumnType::CHAR;
        int m = 0, dummy = 0; bool ok = false;
        parse_from_token(T, m, dummy, false, ok);
        if (!ok) { if (!parse_from_stream(m, dummy, false)) return false; }
        col.length = m;
    }
    // VARCHAR(n)
    else if (T.rfind("VARCHAR", 0) == 0) {
        col.type = EngColumnType::VARCHAR;
        int m = 0, dummy = 0; bool ok = false;
        parse_from_token(T, m, dummy, false, ok);
        if (!ok) {
            // 先尝试 "(m)"
            std::streampos pos = is.tellg();
            if (parse_from_stream(m, dummy, false)) {
                col.length = m;
            }
            else {
                // 再尝试裸的数字：VARCHAR 20
                is.clear(); is.seekg(pos);
                if (!(is >> m)) { return false; }       // 既无括号也无数字，报错
                col.length = m;
            }
        }
        else {
            col.length = m;
        }
    }
    // DECIMAL(p,s)
    else if (T.rfind("DECIMAL", 0) == 0) {
        col.type = EngColumnType::DECIMAL;
        int p = 0, s = 0; bool ok = false;
        parse_from_token(T, p, s, /*two*/true, ok);
        if (!ok) { if (!parse_from_stream(p, s, true)) return false; }
        col.length = p; col.scale = s;
    }
    // TIMESTAMP
    else if (T == "TIMESTAMP") {
        col.type = EngColumnType::TIMESTAMP;
    }
    // FLOAT
    else if (T == "FLOAT") {
        col.type = EngColumnType::FLOAT;
    }
    else {
        return false;
    }

    // 后续可选属性
    while (true) {
        std::streampos p = is.tellg();
        std::string kw; if (!(is >> kw)) break;
        std::string UKW = UP(kw);

        if (UKW == "UNSIGNED") {
            col.unsigned_flag = true;
            continue;
        }
        if (UKW == "NOT") {
            std::string n2;
            if (!(is >> n2) || UP(n2) != "NULL") { is.clear(); is.seekg(p); break; }
            col.not_null = true;
            continue;
        }
        if (UKW == "NULL") {
            col.not_null = false;
            continue;
        }
        if (UKW == "DEFAULT") {
            // 读取一个字面量（数字或带引号的字符串）
            std::string v;
            if (!(is >> v)) { is.clear(); is.seekg(p); break; }
            if (!v.empty() && (v.front() == '\'' || v.front() == '\"')) {
                char q = v.front();
                if (v.back() != q || v.size() == 1) {
                    std::string more, buf = v;
                    while (is >> more) {
                        buf += " " + more;
                        if (!more.empty() && more.back() == q) break;
                    }
                    v = buf;
                }
                if (v.size() >= 2 && v.front() == q && v.back() == q)
                    v = v.substr(1, v.size() - 2);
            }
            col.default_value = v;
            continue;
        }
        if (UKW == "AUTO_INCREMENT") {
            col.auto_increment = true;
            continue;
        }
        if (UKW == "COMMENT") {
            std::string v;
            if (!(is >> v)) { is.clear(); is.seekg(p); break; }
            if (!v.empty() && (v.front() == '\'' || v.front() == '"')) {
                char q = v.front();
                if (v.back() != q || v.size() == 1) {
                    std::string more, buf = v;
                    while (is >> more) {
                        buf += " " + more;
                        if (!more.empty() && more.back() == q) break;
                    }
                    v = buf;
                }
                if (v.size() >= 2 && v.front() == q && v.back() == q)
                    v = v.substr(1, v.size() - 2);
            }
            while (!v.empty() && (v.back() == ';' || isspace((unsigned char)v.back()))) v.pop_back();

            col.comment = v;
            continue;
        }

        // 不认识的词，回退
        is.clear();
        is.seekg(p);
        break;
    }

    return true;
}

// 把列类型转 MySQL 风格字符串（注意 UNSIGNED）
static std::string col_type_to_mysql(const EngColumn& c) {
    const std::string base = [&]() -> std::string {
        switch (c.type) {
        case EngColumnType::INT:       return c.length > 0 ? std::string("int(") + std::to_string(c.length) + ")" : std::string("int");
        case EngColumnType::TINYINT:   return c.length > 0 ? std::string("tinyint(") + std::to_string(c.length) + ")" : std::string("tinyint");
        case EngColumnType::CHAR:      return std::string("char(") + std::to_string(c.length) + ")";
        case EngColumnType::VARCHAR:   return std::string("varchar(") + std::to_string(c.length) + ")";
        case EngColumnType::DECIMAL:   return std::string("decimal(") + std::to_string(c.length) + "," + std::to_string(c.scale) + ")";
        case EngColumnType::TIMESTAMP: return std::string("timestamp");
        case EngColumnType::FLOAT:     return std::string("float");
        }
        return std::string("varchar(0)");
        }();
        if (c.unsigned_flag && (c.type == EngColumnType::INT || c.type == EngColumnType::TINYINT))
            return base + " unsigned";
        return base;
}

// ------- pretty print table -------
// headers.size() == 每行的列数；rows 里每行 cells.size() 也要相同
static void print_boxed_table(const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows) {
    using std::string; using std::vector;

    const size_t n = headers.size();
    std::vector<size_t> w(n, 0);
    for (size_t i = 0; i < n; ++i) w[i] = headers[i].size();
    for (const auto& r : rows) {
        for (size_t i = 0; i < n && i < r.size(); ++i)
            w[i] = std::max(w[i], r[i].size());
    }

    auto line = [&]() {
        std::cout << '+';
        for (size_t i = 0; i < n; ++i) {
            std::cout << std::string(w[i] + 2, '-')
                << (i + 1 == n ? "+\n" : "+");
        }
        };
    auto row = [&](const std::vector<std::string>& r) {
        std::cout << '|';
        for (size_t i = 0; i < n; ++i) {
            std::cout << ' ' << std::left << std::setw((int)w[i])
                << (i < r.size() ? r[i] : "")
                << ' ' << '|';
        }
        std::cout << "\n";
        };

    line();
    row(headers);
    line();
    for (const auto& r : rows) row(r);
    line();
}


// ==========================
// Parse & Dispatch SQL
// ==========================
bool Executor::Execute(const std::string& sql) {
    std::istringstream iss(sql);
    std::string command;
    iss >> command;

    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "CREATE") {
        std::string kw;
        if (!(iss >> kw) || upper1(kw) != "TABLE") {
            std::cerr << "CREATE: expect TABLE\n";
            return false;
        }

        // 读取表名；兼容 "user(" 这种把 '(' 黏在表名上的写法
        std::string tableName;
        if (!(iss >> tableName)) {
            std::cerr << "CREATE TABLE: expect name\n";
            return false;
        }

        bool hasParen = false;
        if (!tableName.empty() && tableName.back() == '(') {
            tableName.pop_back();       // 去掉末尾的 '('
            hasParen = true;
        }

        // 如果上一步没看到 '('，就再读一个 '('（跳过空白）
        if (!hasParen) {
            char ch = 0;
            // operator>> 会跳过空白
            if (!(iss >> ch) || ch != '(') {
                std::cerr << "CREATE TABLE: expect '('\n";
                return false;
            }
        }

        // 把括号里的内容完整读出来（支持多行）
        std::string inside;
        {
            int par = 1;
            char c;
            while (iss.get(c)) {
                if (c == '(') ++par;
                else if (c == ')') {
                    if (--par == 0) break;
                }
                inside.push_back(c);
            }
            if (par != 0) {
                std::cerr << "CREATE TABLE: unbalanced parentheses\n";
                return false;
            }
        }

        std::vector<EngColumn> columns;
        bool pk_seen = false;
        std::string pk_col;

        for (auto item : split_items_respecting_paren(inside)) {
            if (item.empty()) continue;
            std::string upItem = upper1(item);
            if (upItem.rfind("PRIMARY KEY", 0) == 0) {
                auto lp = item.find('(');
                auto rp = item.rfind(')');
                if (lp == std::string::npos || rp == std::string::npos || rp <= lp) {
                    std::cerr << "PRIMARY KEY syntax\n";
                    return false;
                }
                pk_col = trim1(item.substr(lp + 1, rp - lp - 1));
                pk_seen = true;
                continue;
            }
            std::istringstream is(item);
            std::string colName;
            if (!(is >> colName)) {
                std::cerr << "bad column def\n";
                return false;
            }
            EngColumn col;
            col.name = colName;
            if (!parse_type_and_attrs(is, col)) {
                std::cerr << "bad type for column " << colName << "\n";
                return false;
            }
            columns.push_back(col);
        }

        if (pk_seen) {
            for (auto& c : columns)
                if (c.name == pk_col) { c.primary_key = true; c.not_null = true; break; }
        }

        return ExecuteCreateTable(tableName, columns, tableName + ".tbl");
    }

    else if (command == "INSERT") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // INTO tableName
        iss >> tmp;              // VALUES

        // 把余下整行拿出来做“( ... )”内解析，避免把 ')' 或 ';' 混进值
        std::string rest, piece;
        {
            std::ostringstream os;
            while (std::getline(iss, piece)) {
                os << piece;
            }
            rest = os.str();
        }

        // 找到第一个 '(' 和与之匹配的最后一个 ')'
        auto lp = rest.find('(');
        auto rp = rest.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos || rp <= lp) {
            std::cerr << "INSERT parse error: missing parentheses.\n";
            return false;
        }
        std::string inside = rest.substr(lp + 1, rp - lp - 1);

        // 切分逗号
        std::vector<std::string> values;
        {
            std::istringstream vs(inside);
            std::string tok;
            while (std::getline(vs, tok, ',')) {
                // 统一清洗：去首尾空白、去掉尾随的 ';'、去掉首尾引号
                auto ltrim = [](std::string& s) {
                    size_t i = 0;
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
                    if (i) s.erase(0, i);
                    };
                auto rtrim = [](std::string& s) {
                    while (!s.empty()) {
                        char c = s.back();
                        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ';') s.pop_back();
                        else break;
                    }
                    };
                ltrim(tok);
                rtrim(tok);
                // 去包裹引号（支持 'x' 或 "x"）
                if (tok.size() >= 2) {
                    if ((tok.front() == '\'' && tok.back() == '\'') ||
                        (tok.front() == '\"' && tok.back() == '\"')) {
                        tok = tok.substr(1, tok.size() - 2);
                    }
                }
                values.push_back(tok);
            }
        }

        return ExecuteInsert(tableName, values);
    }
    else if (command == "SELECT") {
        std::vector<std::string> cols;
        std::string col;
        while (iss >> col && upper1(col) != "FROM") {
            if (!col.empty() && col.back() == ',') col.pop_back();
            cols.push_back(col);
        }
        std::string tableName; iss >> tableName;
        tableName = trim_semicolon(tableName);   // 去掉表名末尾的 ';'

        std::string whereCol, whereVal, tok;
        if (iss >> tok && upper1(tok) == "WHERE") {
            std::string cond; std::getline(iss, cond);
            cond = trim1(cond);
            cond = trim_semicolon(cond);
            // 支持无空格：id=2 / name="A B"
            auto eq = cond.find('=');
            if (eq != std::string::npos) {
                whereCol = trim1(cond.substr(0, eq));
                whereVal = trim1(cond.substr(eq + 1));
                if (whereVal.size() >= 2 &&
                    ((whereVal.front() == '\'' && whereVal.back() == '\'') ||
                        (whereVal.front() == '"' && whereVal.back() == '"'))) {
                    whereVal = whereVal.substr(1, whereVal.size() - 2);
                }
            }
        }
        return ExecuteSelect(tableName, cols, whereCol, whereVal);
    }
    else if (command == "DELETE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // FROM tableName
        tableName = trim_semicolon(tableName);

        std::string whereCol, whereVal;
        if (iss >> tmp && upper1(tmp) == "WHERE") {
            std::string cond; std::getline(iss, cond);
            cond = trim1(cond);
            cond = trim_semicolon(cond);
            auto eq = cond.find('=');
            if (eq != std::string::npos) {
                whereCol = trim1(cond.substr(0, eq));
                whereVal = trim1(cond.substr(eq + 1));
                if (whereVal.size() >= 2 &&
                    ((whereVal.front() == '\'' && whereVal.back() == '\'') ||
                        (whereVal.front() == '"' && whereVal.back() == '"'))) {
                    whereVal = whereVal.substr(1, whereVal.size() - 2);
                }
            }
        }
        return ExecuteDelete(tableName, whereCol, whereVal);
    }
    else if (command == "DROP") {
        auto up = [](std::string s) {
            for (auto& ch : s) ch = static_cast<char>(std::toupper((unsigned char)ch));
            return s;
            };

        std::string tok;

        // 期望 TABLE（不区分大小写）
        if (!(iss >> tok) || up(tok) != "TABLE") {
            std::cerr << "DROP: expect TABLE\n";
            return false;
        }

        bool if_exists = false;

        // 兼容 "DROP TABLE IF EXISTS t1;" 与 "DROP TABLE t1;"
        std::streampos pos_after_table = iss.tellg();
        std::string maybeIf;
        if (iss >> maybeIf) {
            if (up(maybeIf) == "IF") {
                std::string kwExists;
                if (!(iss >> kwExists) || up(kwExists) != "EXISTS") {
                    std::cerr << "DROP: expect EXISTS after IF\n";
                    return false;
                }
                if_exists = true;
            }
            else {
                // 不是 IF，回退让它作为表名读取
                iss.clear();
                iss.seekg(pos_after_table);
            }
        }

        std::string tableName;
        if (!(iss >> tableName)) {
            std::cerr << "DROP: expect table name\n";
            return false;
        }
        tableName = trim_semicolon(tableName);

        return ExecuteDropTable(tableName, if_exists);
    }
    else if (command == "DESC" || command == "DESCRIBE") {
        std::string t1;
        if (!(iss >> t1)) { std::cerr << "DESC: expect table name\n"; return false; }
        // 兼容可选的 TABLE 关键字
        if (upper1(t1) == "TABLE") {
            if (!(iss >> t1)) { std::cerr << "DESC: expect table name\n"; return false; }
        }
        t1 = trim_semicolon(t1);
        return ExecuteDesc(t1);
    }
    else if (command == "SHOW") {
        auto UP = [](std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; };

        std::string kw1;
        if (!(iss >> kw1)) { std::cerr << "SHOW: expect keyword\n"; return false; }
        kw1 = UP(trim_semicolon(kw1));  // 关键：大写 + 去分号

        if (kw1 == "DATABASES") {
            return ExecuteShowDatabases();
        }
        if (kw1 == "TABLES") {
            ExecuteShowTables();
            return true;
        }
        if (kw1 == "CREATE") {
            std::string kw2, tname;
            if (!(iss >> kw2)) { std::cerr << "SHOW CREATE: expect TABLE\n"; return false; }
            kw2 = UP(trim_semicolon(kw2));
            if (kw2 != "TABLE") { std::cerr << "SHOW CREATE: expect TABLE\n"; return false; }
            if (!(iss >> tname)) { std::cerr << "SHOW CREATE TABLE: expect table name\n"; return false; }
            tname = trim_semicolon(tname);
            return ExecuteShowCreate(tname);
        }

        std::cerr << "SHOW: unsupported form\n";
        return false;
        }
    else if (command == "ALTER") {
        auto up = [](std::string s) { for (auto& c : s) c = (char)std::toupper((unsigned char)c); return s; };

        std::string kwTable;
        if (!(iss >> kwTable) || up(kwTable) != "TABLE") {
            std::cerr << "ALTER: expect TABLE\n"; return false;
        }
        std::string tableName;
        if (!(iss >> tableName)) { std::cerr << "ALTER TABLE: expect table name\n"; return false; }

        std::string action;
        if (!(iss >> action)) { std::cerr << "ALTER TABLE: expect action\n"; return false; }
        action = up(action);

        auto parse_type = [&](ColumnType& ty, int& len)->bool {
            std::string t; if (!(iss >> t)) return false; t = up(t);
            if (t == "INT") { ty = ColumnType::INT;   len = 0; return true; }
            if (t == "FLOAT") { ty = ColumnType::FLOAT; len = 0; return true; }
            if (t == "VARCHAR") {
                if (iss.peek() == '(') { char ch; int L = 0; iss >> ch >> L >> ch; ty = ColumnType::VARCHAR; len = L; return true; }
                int L = 0; if (iss >> L) { ty = ColumnType::VARCHAR; len = L; return true; }
                ty = ColumnType::VARCHAR; len = 0; return true; // 未写长度也给过
            }
            return false;
            };

        if (action == "RENAME") {
            std::string maybeTo, newName;
            std::streampos pos = iss.tellg();
            if (iss >> maybeTo) {
                if (up(maybeTo) == "TO") {
                    if (!(iss >> newName)) { std::cerr << "ALTER TABLE RENAME TO: expect new name\n"; return false; }
                }
                else { iss.clear(); iss.seekg(pos); iss >> newName; }
            }
            else { std::cerr << "ALTER TABLE RENAME: expect new name\n"; return false; }
            newName = trim_semicolon(newName);
            return ExecuteAlterRename(tableName, newName);
        }
        else if (action == "ADD") {
            std::string colName; if (!(iss >> colName)) { std::cerr << "ALTER TABLE ADD: expect column name\n"; return false; }
            std::string rest, token; { std::ostringstream os; while (iss >> token) { if (upper1(token) == "AFTER") { break; } os << token << " "; } rest = os.str(); }
            EngColumn col; col.name = colName;
            { std::istringstream is(rest); if (!parse_type_and_attrs(is, col)) { std::cerr << "ALTER TABLE ADD: bad definition\n"; return false; } }
            std::string afterCol;
            if (upper1(token) == "AFTER") { iss >> afterCol; afterCol = trim_semicolon(afterCol); }
            return ExecuteAlterAdd(tableName, col, afterCol);
        }
        else if (action == "DROP") {
            std::string colName; if (!(iss >> colName)) { std::cerr << "ALTER TABLE DROP: expect column name\n"; return false; }
            colName = trim_semicolon(colName);
            return ExecuteAlterDrop(tableName, colName);
        }
        else if (action == "MODIFY") {
            std::string colName; if (!(iss >> colName)) { std::cerr << "ALTER TABLE MODIFY: expect column name\n"; return false; }
            std::string rest; { std::string piece; std::ostringstream os; while (iss >> piece) os << piece << " "; rest = os.str(); }
            EngColumn col; col.name = colName;
            { std::istringstream is(rest); if (!parse_type_and_attrs(is, col)) { std::cerr << "ALTER TABLE MODIFY: bad definition\n"; return false; } }
            // 你的 CatalogManager::AlterModifyColumn 目前签名是 (type,len)，这里保持不变
            return ExecuteAlterModify(tableName, colName, col.type, col.length);
        }
        else if (action == "CHANGE") {
            std::string oldName, newName;
            if (!(iss >> oldName >> newName)) {
                std::cerr << "ALTER TABLE CHANGE: expect oldName newName\n";
                return false;
            }
            std::string rest;
            {
                std::string piece;
                std::ostringstream os;
                while (iss >> piece) os << piece << " ";
                rest = os.str();
            }
            EngColumn col;
            col.name = newName;
            {
                std::istringstream is(rest);
                if (!parse_type_and_attrs(is, col)) {
                    std::cerr << "ALTER TABLE CHANGE: bad definition\n";
                    return false;
                }
            }
            return ExecuteAlterChange(tableName, oldName, col);
        }

        // 走到这里说明 ALTER 的子命令不被支持
        std::cerr << "ALTER TABLE: unsupported action\n";
        return false;
        }  // <== 结束 if (command == "ALTER")

    else {
            std::cerr << "Unknown command: " << command << "\n";
            return false;
            }
}  // <== 结束 bool Executor::Execute(const std::string& sql)

// ==========================
// CREATE TABLE
// ==========================
bool Executor::ExecuteCreateTable(const std::string& tableName,
    const std::vector<Column>& columns,
    const std::string& fileName) {
    Schema schema(columns);
    if (!catalogManager.CreateTable(catalog, tableName, schema, fileName)) return false;

    // 用存储层初始化页链
    if (!storage.InitTablePages(tableName)) {
        std::cerr << "Storage Init failed.\n";
        return false;
    }
    std::cout << "Table '" << tableName << "' created.\n";
    return true;
}

// ==========================
// INSERT
// ==========================
bool Executor::ExecuteInsert(const std::string& tableName,
    const std::vector<std::string>& values) {
    TableInfo* table = catalog.GetTable(tableName);
    if (!table) { std::cerr << "Error: table " << tableName << " not found.\n"; return false; }

    if (!storage.Insert(tableName, values)) {
        std::cerr << "Insert failed.\n"; return false;
    }
    return true;
}

// ==========================
// SELECT
// ==========================
bool Executor::ExecuteSelect(const std::string& tableName,
    const std::vector<std::string>& columns,
    const std::string& whereCol,
    const std::string& whereVal) {

    TableInfo* table = catalog.GetTable(tableName);
    if (!table) { std::cerr << "Error: table " << tableName << " not found.\n"; return false; }

    // 拉所有数据（你的存储层返回字符串向量）
    auto rows_raw = storage.SelectAll(tableName);

    // where 过滤
    int whereIdx = -1;
    if (!whereCol.empty()) {
        const auto& cols = table->getSchema().GetColumns();
        for (int i = 0; i < (int)cols.size(); ++i)
            if (cols[i].name == whereCol) { whereIdx = i; break; }
        if (whereIdx == -1) { std::cerr << "Error: WHERE column not found.\n"; return false; }
    }

    // 决定投影列索引 & 表头
    std::vector<int> projIdx;
    std::vector<std::string> headers;
    const auto& schemaCols = table->getSchema().GetColumns();
    bool star = (columns.size() == 1 && columns[0] == "*");
    if (star) {
        for (int i = 0; i < (int)schemaCols.size(); ++i) {
            projIdx.push_back(i);
            headers.push_back(schemaCols[i].name);
        }
    }
    else {
        for (const auto& c : columns) {
            int idx = -1;
            for (int i = 0; i < (int)schemaCols.size(); ++i)
                if (schemaCols[i].name == c) { idx = i; break; }
            if (idx == -1) { std::cerr << "Error: column " << c << " not found.\n"; return false; }
            projIdx.push_back(idx);
            headers.push_back(c);
        }
    }

    // 生成展示行（先 where，再投影）
    std::vector<std::vector<std::string>> outRows;
    for (const auto& fields : rows_raw) {
        if (whereIdx >= 0) {
            if (whereIdx >= (int)fields.size()) continue;
            if (fields[whereIdx] != whereVal) continue;
        }
        std::vector<std::string> out;
        out.reserve(projIdx.size());
        for (int idx : projIdx) {
            out.push_back(idx < (int)fields.size() ? fields[idx] : "");
        }
        outRows.push_back(std::move(out));
    }

    // 表格打印
    if (headers.empty()) {          // 没有列就直接空
        std::cout << "(empty)\n";
    }
    else {
        print_boxed_table(headers, outRows);
    }
    return true;
}



// ==========================
// DELETE
// ==========================
bool Executor::ExecuteDelete(const std::string& tableName,
    const std::string& whereCol,
    const std::string& whereVal) {
    TableInfo* table = catalog.GetTable(tableName);
    if (!table) { std::cerr << "Error: table " << tableName << " not found.\n"; return false; }

    int whereIdx = -1;
    if (!whereCol.empty()) {
        const auto& cols = table->getSchema().GetColumns();
        for (int i = 0; i < (int)cols.size(); ++i)
            if (cols[i].name == whereCol) { whereIdx = i; break; }
        if (whereIdx == -1) { std::cerr << "Error: WHERE column not found.\n"; return false; }
    }

    if (!storage.DeleteWhere(tableName, whereIdx, whereVal)) {
        std::cerr << "Delete failed.\n"; return false;
    }
    std::cout << "[RecordManager] Delete finished.\n";
    return true;
}


// ==========================
// DROP TABLE
// ==========================
// executor.cpp —— ExecuteDropTable 改成带 if_exists
bool Executor::ExecuteDropTable(const std::string& tableName, bool if_exists) {
    // 目录查一下（如果你手头有 catalog）
    TableInfo* t = catalog.GetTable(tableName);
    if (!t) {
        if (if_exists) {
            std::cout << "Table '" << tableName << "' does not exist. Ignored.\n";
            return true;
        }
        std::cerr << "Error: table " << tableName << " not found.\n";
        return false;
    }

    // 1) 删除数据（当前最小实现为 no-op，已经在 storage_engine.cpp 添加了 DropTableData）
    storage.DropTableData(tableName);

    // 2) 删除目录项
    if (catalogManager.DropTable(catalog, tableName)) {
        std::cout << "Table '" << tableName << "' dropped.\n";
        return true;
    }
    // 兜底：如果失败
    if (if_exists) {
        std::cout << "Table '" << tableName << "' does not exist. Ignored.\n";
        return true;
    }
    return false;
}


bool Executor::ExecuteDesc(const std::string& tableName) {
    TableInfo* t = catalog.GetTable(tableName);
    if (!t) { std::cerr << "Error: table " << tableName << " not found.\n"; return false; }

    // 固定列宽
    const int W_FIELD = 12;
    const int W_TYPE = 21;
    const int W_NULL = 4;
    const int W_KEY = 10;
    const int W_DEF = 17;
    const int W_EXTRA = 25;

    auto line = [&]() {
        std::cout << '+'
            << std::string(W_FIELD + 2, '-') << '+'
            << std::string(W_TYPE + 2, '-') << '+'
            << std::string(W_NULL + 2, '-') << '+'
            << std::string(W_KEY + 2, '-') << '+'
            << std::string(W_DEF + 2, '-') << '+'
            << std::string(W_EXTRA + 2, '-') << "+\n";
        };

    auto cell = [](const std::string& s, int w) {
        std::cout << " " << std::left << std::setw(w) << s << " ";
        };

    line();
    std::cout << '|'; cell("Field", W_FIELD);
    std::cout << '|'; cell("Type", W_TYPE);
    std::cout << '|'; cell("Null", W_NULL);
    std::cout << '|'; cell("Key", W_KEY);
    std::cout << '|'; cell("Default", W_DEF);
    std::cout << '|'; cell("Extra", W_EXTRA);
    std::cout << "|\n";
    line();

    for (const auto& c : t->getSchema().GetColumns()) {
        std::string ty = upper1(col_type_to_mysql(c));
        std::string Null = c.not_null ? "NO" : "YES";
        std::string Key = c.primary_key ? "PRI" : "";
        std::string Def = c.default_value.empty() ? "NULL" : c.default_value;
        std::string Extra;
        if (c.auto_increment) Extra = "auto_increment";

        std::cout << '|'; cell(c.name, W_FIELD);
        std::cout << '|'; cell(ty, W_TYPE);
        std::cout << '|'; cell(Null, W_NULL);
        std::cout << '|'; cell(Key, W_KEY);
        std::cout << '|'; cell(Def, W_DEF);
        std::cout << '|'; cell(Extra, W_EXTRA);
        std::cout << "|\n";
    }
    line();
    return true;
}


bool Executor::ExecuteShowCreate(const std::string& tableName) {
    TableInfo* t = catalog.GetTable(tableName);
    if (!t) { std::cerr << "Error: table " << tableName << " not found.\n"; return false; }

    const auto& cols = t->getSchema().GetColumns();
    std::ostringstream ddl;
    ddl << "CREATE TABLE " << t->name << " (\n";
    bool first = true; std::string pk;
    for (const auto& c : cols) {
        if (!first) ddl << ",\n";
        first = false;
        ddl << "  " << c.name << " " << upper1(col_type_to_mysql(c));
        if (c.not_null) ddl << " NOT NULL"; else ddl << " NULL";
        if (!c.default_value.empty()) ddl << " DEFAULT " << c.default_value;
        if (c.auto_increment) ddl << " AUTO_INCREMENT";
        if (!c.comment.empty()) ddl << " COMMENT \"" << c.comment << "\"";
        if (c.primary_key) pk = c.name;
    }
    if (!pk.empty()) ddl << ",\n  PRIMARY KEY(" << pk << ")";
    ddl << "\n);";
    std::cout << ddl.str() << "\n";
    return true;
}


bool Executor::ExecuteAlterRename(const std::string& oldName, const std::string& newName) {
    if (catalogManager.RenameTable(catalog, oldName, newName)) {
        std::cout << "Table '" << oldName << "' renamed to '" << newName << "'.\n";
        return true;
    }
    std::cerr << "ALTER TABLE RENAME failed.\n";
    return false;
}

bool Executor::ExecuteAlterAdd(const std::string& tableName, const Column& col, const std::string& after) {
    if (catalogManager.AlterAddColumn(catalog, tableName, col, after)) {
        std::cout << "Column '" << col.name << "' added.\n";
        return true;
    }
    std::cerr << "ALTER TABLE ADD failed.\n";
    return false;
}

bool Executor::ExecuteAlterDrop(const std::string& tableName, const std::string& colName) {
    if (catalogManager.AlterDropColumn(catalog, tableName, colName)) {
        std::cout << "Column '" << colName << "' dropped.\n";
        return true;
    }
    std::cerr << "ALTER TABLE DROP failed.\n";
    return false;
}

bool Executor::ExecuteAlterModify(const std::string& tableName, const std::string& colName,
    ColumnType ty, int len) {
    if (catalogManager.AlterModifyColumn(catalog, tableName, colName, ty, len)) {
        std::cout << "Column '" << colName << "' modified.\n";
        return true;
    }
    std::cerr << "ALTER TABLE MODIFY failed.\n";
    return false;
}

bool Executor::ExecuteAlterChange(const std::string& tableName, const std::string& oldName,
    const Column& newDef) {
    if (catalogManager.AlterChangeColumn(catalog, tableName, oldName, newDef)) {
        std::cout << "Column '" << oldName << "' changed to '" << newDef.name << "'.\n";
        return true;
    }
    std::cerr << "ALTER TABLE CHANGE failed.\n";
    return false;
}

// ==========================
// SHOW TABLES
// ==========================
void Executor::ExecuteShowTables() {
    // 取所有表名
    std::vector<std::vector<std::string>> rows;
    for (const auto& tableName : catalog.ListTables()) {
        rows.push_back({ tableName });
    }
    // 表头：保持简单叫 "Tables"
    print_boxed_table({ "Tables_in_" + current_db }, rows);
}

// engine/executor.cpp（合适位置，和其他 ExecuteXxx 同级）
bool Executor::ExecuteShowDatabases() {
    namespace fs = std::filesystem;
    fs::create_directories("data");
    std::cout << "Databases:\n";
    for (auto& entry : fs::directory_iterator("data")) {
        if (entry.is_directory()) {
            std::cout << " - " << entry.path().filename().string() << "\n";
        }
    }
    return true;
}

namespace {

    // 找到子树里第一个 type 的节点
    static const minidb::PlanNode* find_node(const minidb::PlanNode* n, minidb::PlanOp target) {
        if (!n) return nullptr;
        if (n->op == target) return n;
        for (auto& ch : n->children) {
            if (auto* hit = find_node(ch.get(), target)) return hit;
        }
        return nullptr;
    }

    // 从 FILTER 节点里提取 “col = const” （简单版，只为兼容现有 Executor 的 where 形参）
    static bool extract_simple_eq(const minidb::Expr* e, std::string& col, std::string& val) {
        using namespace minidb;
        auto* cmp = dynamic_cast<const CmpExpr*>(e);
        if (!cmp) return false;
        if (!(cmp->op == CmpOp::EQ || cmp->op == CmpOp::GE || cmp->op == CmpOp::LE
            || cmp->op == CmpOp::GT || cmp->op == CmpOp::LT)) return false;

        auto* lhs = dynamic_cast<const ColRef*>(cmp->lhs.get());
        if (!lhs) return false;

        if (auto* rint = dynamic_cast<const IntLit*>(cmp->rhs.get())) {
            col = lhs->name;
            val = std::to_string(rint->v);
            return true;
        }
        if (auto* rstr = dynamic_cast<const StrLit*>(cmp->rhs.get())) {
            col = lhs->name;
            val = rstr->v;
            return true;
        }
        return false;
    }

} // anonymous

bool Executor::ExecutePlan(const minidb::Plan& plan) {
    using namespace minidb;
    if (!plan.root) {
        std::cerr << "Empty plan.\n";
        return false;
    }

    const PlanNode* root = plan.root.get();

    switch (root->op) {
    case PlanOp::CREATE: {
        // root 携带 TableDef
        std::vector<::Column> cols;  // 注意：这里用 engine 的 Column（全局命名）
        for (auto& c : root->create_def.columns) {
            ColumnType t = (c.type == DataType::INT32) ? ColumnType::INT : ColumnType::VARCHAR;
            cols.emplace_back(c.name, t, /*len*/64, false, false);
        }
        return ExecuteCreateTable(root->create_def.name, cols, root->create_def.name + ".tbl");
    }
    case PlanOp::INSERT: {
    // root.table + root.insert_values (注意：你们 planner 里是 vector<unique_ptr<Expr>>)
    std::vector<std::string> values;
    values.reserve(root->insert_values.size());

    for (auto& uptr : root->insert_values) {
        const minidb::Expr* e = uptr.get();
        if (auto pInt = dynamic_cast<const minidb::IntLit*>(e)) {
            values.push_back(std::to_string(pInt->v));
        } else if (auto pStr = dynamic_cast<const minidb::StrLit*>(e)) {
            values.push_back(pStr->v);
        } else if (auto pCol = dynamic_cast<const minidb::ColRef*>(e)) {
            // 如果 INSERT 里写了标识符，先按字面塞进去（你们后端是字符串向 RecordManager 打印）
            values.push_back(pCol->name);
        } else {
            // 兜底：不可识别表达式
            std::cerr << "Unsupported INSERT value expression.\n";
            return false;
        }
    }
    return ExecuteInsert(root->table, values);
}

    case PlanOp::PROJECT: {
        // SELECT：PROJECT(Filter?(SeqScan))
        const PlanNode* scan = find_node(root, PlanOp::SEQSCAN);
        if (!scan) { std::cerr << "Bad plan: no SEQSCAN under PROJECT.\n"; return false; }
        std::string whereCol, whereVal;
        if (const PlanNode* fil = find_node(root, PlanOp::FILTER)) {
            if (fil->predicate) extract_simple_eq(fil->predicate.get(), whereCol, whereVal);
        }
        // 列：空 => * ，否则按 root->project
        std::vector<std::string> cols = root->project.empty()
            ? std::vector<std::string>{ "*" }
        : root->project;
        return ExecuteSelect(scan->table, cols, whereCol, whereVal);
    }
    case PlanOp::FILTER: {
        // 正常 Planner 下 FILTER 不会是根，但仍做个兜底
        const PlanNode* scan = find_node(root, PlanOp::SEQSCAN);
        if (!scan) { std::cerr << "Bad plan: FILTER without SEQSCAN.\n"; return false; }
        std::string whereCol, whereVal;
        if (root->predicate) extract_simple_eq(root->predicate.get(), whereCol, whereVal);
        std::vector<std::string> cols = { "*" };
        return ExecuteSelect(scan->table, cols, whereCol, whereVal);
    }
    case PlanOp::SEQSCAN: {
        // 兜底：等价 SELECT * FROM table;
        std::vector<std::string> cols = { "*" };
        return ExecuteSelect(root->table, cols, "", "");
    }
    case PlanOp::DELETE_: {
        // DELETE FROM table WHERE col = val
        std::string whereCol, whereVal;
        if (root->predicate) extract_simple_eq(root->predicate.get(), whereCol, whereVal);
        return ExecuteDelete(root->table, whereCol, whereVal);
    }
    case PlanOp::DROP: {
        // 使用编译器路径：执行 DROP
        // root->table 是要删的表名；root->if_exists 标识 IF EXISTS
        return ExecuteDropTable(root->table, root->if_exists);
    }
    default:
        std::cerr << "Unsupported plan root.\n";
        return false;
    }
}
