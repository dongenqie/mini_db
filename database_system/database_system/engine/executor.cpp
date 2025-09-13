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

// ==========================
// Parse & Dispatch SQL
// ==========================
bool Executor::Execute(const std::string& sql) {
    std::istringstream iss(sql);
    std::string command;
    iss >> command;

    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "CREATE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // TABLE tableName

        // Parse danh sách cột: (colName type [len], ...)
        char ch;
        iss >> ch; // (
        std::vector<Column> columns;
        while (iss.peek() != ')' && iss.good()) {
            std::string colName, typeStr;
            int len = 0;
            iss >> colName >> typeStr;

            ColumnType type;
            if (typeStr == "INT") type = ColumnType::INT;
            else if (typeStr == "FLOAT") type = ColumnType::FLOAT;
            else {
                type = ColumnType::VARCHAR;
                iss >> len; // lấy độ dài
            }

            columns.emplace_back(colName, type, len);

            if (iss.peek() == ',') iss.ignore();
        }
        iss >> ch; // )

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
        while (iss >> col && col != "FROM") {
            if (col.back() == ',') col.pop_back();
            cols.push_back(col);
        }
        std::string tableName;
        iss >> tableName;
        std::string whereCol, whereVal, tmp;
        if (iss >> tmp && tmp == "WHERE") {
            iss >> whereCol >> tmp >> whereVal; // col = value
        }
        return ExecuteSelect(tableName, cols, whereCol, whereVal);
    }
    else if (command == "DELETE") {
        std::string tmp, tableName;
        iss >> tmp >> tableName; // FROM tableName
        std::string whereCol, whereVal;
        if (iss >> tmp && tmp == "WHERE") {
            iss >> whereCol >> tmp >> whereVal; // col = value
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

    else if (command == "SHOW") {
        ExecuteShowTables();
        return true;
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
        return false;
    }
}

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
    std::cout << "1 row inserted.\n";   // 新增：插入成功提示
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

    auto rows = storage.SelectAll(tableName);

    // where 过滤 + 投影（与原 CSV 版本一致）
    int whereIdx = -1;
    if (!whereCol.empty()) {
        const auto& cols = table->getSchema().GetColumns();
        for (int i = 0; i < (int)cols.size(); ++i)
            if (cols[i].name == whereCol) { whereIdx = i; break; }
        if (whereIdx == -1) { std::cerr << "Error: WHERE column not found.\n"; return false; }
    }

    std::vector<int> projIdx;
    bool star = (columns.size() == 1 && columns[0] == "*");
    if (!star) {
        const auto& cols = table->getSchema().GetColumns();
        for (auto& c : columns) {
            int idx = -1;
            for (int i = 0; i < (int)cols.size(); ++i)
                if (cols[i].name == c) { idx = i; break; }
            if (idx == -1) { std::cerr << "Error: column " << c << " not found.\n"; return false; }
            projIdx.push_back(idx);
        }
    }

    for (auto& fields : rows) {
        if (whereIdx >= 0) {
            if (whereIdx >= (int)fields.size()) continue;
            if (fields[whereIdx] != whereVal) continue;
        }

        if (star) {
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i) std::cout << " | ";
                std::cout << fields[i];
            }
            std::cout << "\n";
        }
        else {
            for (size_t k = 0; k < projIdx.size(); ++k) {
                if (k) std::cout << " | ";
                int idx = projIdx[k];
                std::cout << (idx < (int)fields.size() ? fields[idx] : "");
            }
            std::cout << "\n";
        }
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




// ==========================
// SHOW TABLES
// ==========================
void Executor::ExecuteShowTables() {
    std::cout << "Tables in catalog:\n";
    for (const auto& tableName : catalog.ListTables()) {
        std::cout << " - " << tableName << "\n";
    }
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
