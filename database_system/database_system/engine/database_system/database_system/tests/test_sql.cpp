// =============================================
// test_sql/test_sql.cpp
// =============================================
#include <iostream>
#include <vector>
#include <cassert>

#include "../utils/common.h"

#include "../sql_compiler/lexer.h"
#include "../sql_compiler/parser.h"
#include "../sql_compiler/semantic.h"
#include "../sql_compiler/planner.h"
#include "../sql_compiler/pretty.h"

// 独立目录接口（避免依赖 engine/）
#include "../sql_compiler/catalog_iface.h"

using namespace minidb;

// 简单切分多语句（考虑字符串中的分号）
// 替换原来的 split_sql(...) 为带行号的版本
static std::vector<std::pair<std::string, int>>
split_sql_with_lines(const std::string& all) {
    std::vector<std::pair<std::string, int>> out;

    size_t i = 0;
    int line = 1;
    const int n = (int)all.size();

    auto skip_spaces_and_newlines = [&](size_t& j, int& ln) {
        while (j < all.size()) {
            char c = all[j];
            if (c == ' ' || c == '\t' || c == '\r') { j++; }
            else if (c == '\n') { j++; ln++; }
            else break;
        }
        };

    while (i < all.size()) {
        // 跳过前导空白并更新行号，得到这一条语句的起始位置与行号
        skip_spaces_and_newlines(i, line);
        if (i >= all.size()) break;

        int start_line = line;
        size_t start = i;

        bool in_str = false;
        bool escaped = false;

        // 从 start 扫到分号（不在字符串字面量内的那个）
        for (; i < all.size(); ++i) {
            char c = all[i];

            if (c == '\n') {
                if (!in_str) line++;
                // 在字符串内遇到换行也可以按原样计数（常见 SQL 通常不允许跨行单引号字符串，但我们容忍）
            }

            if (in_str) {
                if (!escaped && c == '\\') { escaped = true; continue; }
                if (!escaped && c == '\'') { in_str = false; }
                escaped = false;
                continue;
            }

            if (c == '\'') { in_str = true; continue; }

            if (c == ';') {
                // 取 [start, i]（包含分号）
                out.emplace_back(all.substr(start, i - start + 1), start_line);
                i++; // 移到分号之后的位置，继续下一条
                break;
            }
        }

        if (i >= all.size()) {
            // 末尾没有分号但还有残留：也输出一条（让语法阶段报“缺分号”）
            std::string tail = all.substr(start);
            if (!trim(tail).empty()) out.emplace_back(tail, start_line);
            break;
        }
    }

    return out;
}

static void run_one(const std::string& sql_src, int start_line, ICatalog& cat) {
    // 不再 trim 前导换行；保持列号=1，且行号从 start_line 起
    const std::string& sql = sql_src;

    std::cout << "============================\n";
    std::cout << "SQL: " << sql << "\n";

    // 1) 词法：从指定行号开始
    {
        std::cout << "TOKENS:\n";
        Lexer lx(sql, start_line, /*start_col*/0, /*keep_comments*/true);
        while (true) {
            Token t = lx.next();
            // >>> 按你要的格式输出：
            std::cout << "  (type=\"" << TokName(t.type)
                << "\", value=\"" << t.lexeme
                << "\", line=" << t.line
                << ", column=" << t.col << ")\n";
            if (t.type == TokenType::END) break;
            if (t.type == TokenType::INVALID) {
                std::cout << "  [LEX ERROR] at " << t.line << ":" << t.col
                    << " : " << t.lexeme << "\n";
                return;
            }
        }

        //（可选）如果你还想输出课程“四元式”，保留下面两行：
        std::cout << "TOKENS (quads):\n";
        PrintTokenQuads(sql, start_line, std::cout);
    }

    // 2) 语法（AST）
    Status pst = Status::OK();
    Lexer lx(sql, start_line);
    Parser ps(lx);
    ps.enable_trace(true);                 // 开启步骤跟踪
    auto stmt = ps.parse_statement(pst);

    // 打印“语法分析步骤四元式”
    {
        std::cout << "SYNTAX TRACE:\n";
        const auto& tr = ps.trace_log();
        for (size_t i = 0; i < tr.size(); ++i) {
            std::cout << "[" << (i + 1) << "] " << tr[i] << "\n";
        }
    }

    if (!pst.ok) {
        std::cout << "SYNTAX ERROR: " << pst.message << "\n";
        return;
    }
    PrintAST(stmt.get(), std::cout);

    // 3) 语义
    SemanticAnalyzer sa(cat);
    auto sem = sa.analyze(stmt.get());
    if (!sem.status.ok) {
        std::cout << "SEMANTIC ERROR: " << sem.status.message << "\n";
        return;
    }
    else {
        std::cout << "SEMANTIC: OK\n";
    }

    // 4) 计划
    Planner pl;
    auto plan = pl.plan_from_stmt(stmt.get());
    PrintPlan(plan, std::cout);
}

int main() {
    InMemoryCatalog cat;

    TableDef td; td.name = "student";
    td.columns = { {"id",DataType::INT32}, {"name",DataType::VARCHAR}, {"age",DataType::INT32} };
    cat.create_table(td);

    // ① 程序实例用例
    //std::string demo =
    //    "/* test_sql_program */\n"
    //    "SELECT name, age\n"
    //    "FROM Students\n"
    //    "WHERE age > 20; -- only adults\n";
    //run_one(demo, /*start_line*/1, cat);

     //② 正确用例（保持你原来的多行字符串）
    std::string ok_sql =
        "SELECT name FROM student;";
    //    "CREATE TABLE course(cid INT, title VARCHAR);\n"
    //    "INSERT INTO student(id,name,age) VALUES (1,'Alice',20);\n"
    //    "SELECT id,name FROM student WHERE age > 18;\n"
    //    "DELETE FROM student WHERE id = 1;\n";

    auto parts = split_sql_with_lines(ok_sql);
    for (auto& [sql, line] : parts) {
        if (!trim(sql).empty())
            run_one(sql, line, cat);
    }

 //   // ③ 错误用例（同样支持跨行定位）
 //   std::vector<std::string> bad = {
 //       "SELECT id FROM student",                          // 缺分号
 //       "SELECT idx,name FROM student;",                   // 列名拼写错误
 //       "INSERT INTO student(id,name,age) VALUES (2,20,19);", // 类型不匹配
 //       "INSERT INTO student(id,name,age) VALUES (3,'Bob');", // 值个数不一致
 //       "INSERT INTO student(id,name,age) VALUES (3,'Bob,19);", // 未闭合字符串
 //       "CREATE TABLE t(a INT, a VARCHAR);"                // 重复列
 //   };

 //   int line = 1;
 //   for (auto& s : bad) {
 //       run_one(s, line, cat);
 //       line += 1; // 让每条错误用例也能看到不同的起始行号
 //   }



    std::cout << "==== SQL compiler end ====\n";
    return 0;
}