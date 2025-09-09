// =============================================
// sql_compiler/test_sql.cpp
// =============================================
#include <cassert>
#include <iostream>

#include "../utils/common.h"
#include "../sql_compiler/lexer.h"
#include "../sql_compiler/parser.h"
#include "../sql_compiler/semantic.h"
#include "../sql_compiler/planner.h"
#include "../sql_compiler/catalog_iface.h"   // 用内存目录

using namespace minidb;

static void run_case(const std::string& sql, ICatalog& cat) {
    Lexer lx(sql);
    Parser ps(lx);
    Status st = Status::OK();
    auto stmt = ps.parse_statement(st);
    if (!st.ok) { std::cout << "PARSE ERROR: " << st.message << "\n"; return; }

    SemanticAnalyzer sa(cat);
    auto sem = sa.analyze(stmt.get());
    if (!sem.status.ok) { std::cout << "SEMANTIC ERROR: " << sem.status.message << "\n"; return; }

    Planner pl; auto plan = pl.plan_from_stmt(stmt.get());
    std::cout << "OK: planned root generated.\n";
}

int main() {
    InMemoryCatalog cat;

    // 先注册 student（模拟 CREATE 后的目录状态）
    TableDef td; td.name = "student";
    td.columns = { {"id",DataType::INT32}, {"name",DataType::VARCHAR}, {"age",DataType::INT32} };
    cat.create_table(td);

    // 基本用例
    run_case("SELECT id,name FROM student WHERE age > 18;", cat);
    run_case("INSERT INTO student(id,name,age) VALUES (1,'Alice',20);", cat);
    run_case("DELETE FROM student WHERE id = 1;", cat);

    // CREATE（语义检查：已存在将报错）
    {
        Lexer lx("CREATE TABLE student(id INT, name VARCHAR, age INT);");
        Parser ps(lx); Status st = Status::OK(); auto s = ps.parse_statement(st);
        SemanticAnalyzer sa(cat); auto sem = sa.analyze(s.get());
        assert(!sem.status.ok); // student 已存在
    }

    // 错误用例：缺分号
    {
        Lexer lx("SELECT id FROM student");
        Parser ps(lx); Status st = Status::OK(); auto s = ps.parse_statement(st);
        assert(!st.ok);
    }

    // 错误用例：类型不匹配（把 name 写成数字）
    {
        Lexer lx("INSERT INTO student(id,name,age) VALUES (2,20,19);");
        Parser ps(lx); Status st = Status::OK(); auto s = ps.parse_statement(st);
        SemanticAnalyzer sa(cat); auto sem = sa.analyze(s.get());
        assert(!sem.status.ok);
        std::cout << "Caught type mismatch as expected.\n";
    }

    std::cout << "sql_compiler standalone tests finished.\n";
    return 0;
}