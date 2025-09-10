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

// ����Ŀ¼�ӿڣ��������� engine/��
#include "../sql_compiler/catalog_iface.h"

using namespace minidb;

// ���зֶ���䣨�����ַ����еķֺţ�
// �滻ԭ���� split_sql(...) Ϊ���кŵİ汾
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
        // ����ǰ���հײ������кţ��õ���һ��������ʼλ�����к�
        skip_spaces_and_newlines(i, line);
        if (i >= all.size()) break;

        int start_line = line;
        size_t start = i;

        bool in_str = false;
        bool escaped = false;

        // �� start ɨ���ֺţ������ַ����������ڵ��Ǹ���
        for (; i < all.size(); ++i) {
            char c = all[i];

            if (c == '\n') {
                if (!in_str) line++;
                // ���ַ�������������Ҳ���԰�ԭ������������ SQL ͨ����������е������ַ��������������̣�
            }

            if (in_str) {
                if (!escaped && c == '\\') { escaped = true; continue; }
                if (!escaped && c == '\'') { in_str = false; }
                escaped = false;
                continue;
            }

            if (c == '\'') { in_str = true; continue; }

            if (c == ';') {
                // ȡ [start, i]�������ֺţ�
                out.emplace_back(all.substr(start, i - start + 1), start_line);
                i++; // �Ƶ��ֺ�֮���λ�ã�������һ��
                break;
            }
        }

        if (i >= all.size()) {
            // ĩβû�зֺŵ����в�����Ҳ���һ�������﷨�׶α���ȱ�ֺš���
            std::string tail = all.substr(start);
            if (!trim(tail).empty()) out.emplace_back(tail, start_line);
            break;
        }
    }

    return out;
}

static void run_one(const std::string& sql_src, int start_line, ICatalog& cat) {
    // ���� trim ǰ�����У������к�=1�����кŴ� start_line ��
    const std::string& sql = sql_src;

    std::cout << "============================\n";
    std::cout << "SQL: " << sql << "\n";

    // 1) �ʷ�����ָ���кſ�ʼ
    {
        std::cout << "TOKENS:\n";
        Lexer lx(sql, start_line, /*start_col*/0, /*keep_comments*/true);
        while (true) {
            Token t = lx.next();
            // >>> ����Ҫ�ĸ�ʽ�����
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

        //����ѡ������㻹������γ̡���Ԫʽ���������������У�
        std::cout << "TOKENS (quads):\n";
        PrintTokenQuads(sql, start_line, std::cout);
    }

    // 2) �﷨��AST��
    Status pst = Status::OK();
    Lexer lx(sql, start_line);
    Parser ps(lx);
    ps.enable_trace(true);                 // �����������
    auto stmt = ps.parse_statement(pst);

    // ��ӡ���﷨����������Ԫʽ��
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

    // 3) ����
    SemanticAnalyzer sa(cat);
    auto sem = sa.analyze(stmt.get());
    if (!sem.status.ok) {
        std::cout << "SEMANTIC ERROR: " << sem.status.message << "\n";
        return;
    }
    else {
        std::cout << "SEMANTIC: OK\n";
    }

    // 4) �ƻ�
    Planner pl;
    auto plan = pl.plan_from_stmt(stmt.get());
    PrintPlan(plan, std::cout);
}

int main() {
    InMemoryCatalog cat;

    TableDef td; td.name = "student";
    td.columns = { {"id",DataType::INT32}, {"name",DataType::VARCHAR}, {"age",DataType::INT32} };
    cat.create_table(td);

    // �� ����ʵ������
    //std::string demo =
    //    "/* test_sql_program */\n"
    //    "SELECT name, age\n"
    //    "FROM Students\n"
    //    "WHERE age > 20; -- only adults\n";
    //run_one(demo, /*start_line*/1, cat);

     //�� ��ȷ������������ԭ���Ķ����ַ�����
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

 //   // �� ����������ͬ��֧�ֿ��ж�λ��
 //   std::vector<std::string> bad = {
 //       "SELECT id FROM student",                          // ȱ�ֺ�
 //       "SELECT idx,name FROM student;",                   // ����ƴд����
 //       "INSERT INTO student(id,name,age) VALUES (2,20,19);", // ���Ͳ�ƥ��
 //       "INSERT INTO student(id,name,age) VALUES (3,'Bob');", // ֵ������һ��
 //       "INSERT INTO student(id,name,age) VALUES (3,'Bob,19);", // δ�պ��ַ���
 //       "CREATE TABLE t(a INT, a VARCHAR);"                // �ظ���
 //   };

 //   int line = 1;
 //   for (auto& s : bad) {
 //       run_one(s, line, cat);
 //       line += 1; // ��ÿ����������Ҳ�ܿ�����ͬ����ʼ�к�
 //   }



    std::cout << "==== SQL compiler end ====\n";
    return 0;
}