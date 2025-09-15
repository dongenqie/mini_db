// =============================================
// cli/main.cpp  ��
// =============================================
// cli/main.cpp
#include <iostream>
#include <string>
#include <algorithm>
#include <filesystem>
#include <sstream>

#define MINIDB_IMPL_LEXER
#define MINIDB_IMPL_PARSER
#define MINIDB_IMPL_CATALOG
#define MINIDB_IMPL_SEMANTIC
#define MINIDB_IMPL_PLANNER
#define MINIDB_IMPL_BUFFER
#define MINIDB_IMPL_PAGE
#define MINIDB_IMPL_EXECUTOR

#include "../sql_compiler/lexer.h"
#include "../sql_compiler/parser.h"
#include "../sql_compiler/semantic.h"
#include "../sql_compiler/planner.h"
#include "../sql_compiler/catalog_adapter_engine.h"

#include "../engine/catalog_manager.hpp"
#include "../engine/storage_engine.hpp"
#include "../engine/executor.hpp"

#include "../storage/file_manager.hpp"   // ����
#include "../storage/cache_manager.hpp"  // �õ� ReplacePolicy ö��

using namespace minidb;
namespace fs = std::filesystem;

static inline std::string trim_copy(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char)s[b])) ++b;
    while (e > b && isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

static inline std::string upper_copy(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

static inline bool ends_with_semicolon(const std::string& s) {
    for (int i = (int)s.size() - 1; i >= 0; --i) {
        if (!isspace((unsigned char)s[i])) return s[i] == ';';
    }
    return false;
}

// �г� data/ �����С����ݿ�Ŀ¼�������� catalog.txt ��ǿ�Ŀ¼��
static std::vector<std::string> list_databases(const std::string& root = "data") {
    std::vector<std::string> out;
    if (!fs::exists(root)) return out;
    for (auto& de : fs::directory_iterator(root)) {
        if (!de.is_directory()) continue;
        auto db = de.path().filename().string();
        // ������ catalog.txt ���߿�Ŀ¼Ҳ��⣨��Ҳ�������Ʊ����� catalog.txt��
        out.push_back(db);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ȷ�� data/<db>/ Ŀ¼����
static bool ensure_db_dir(const std::string& db) {
    fs::path p = fs::path("data") / db;
    std::error_code ec;
    if (fs::exists(p, ec)) return true;
    return fs::create_directories(p, ec);
}

static void print_rows(const std::vector<Row>& rows, const TableDef* /*opt_td*/) {
    if (rows.empty()) { std::cout << "(empty)\n"; return; }
    for (const auto& r : rows) {
        for (size_t i = 0; i < r.values.size(); ++i) {
            if (i) std::cout << " | ";
            const auto& v = r.values[i];
            if (std::holds_alternative<int32_t>(v)) std::cout << std::get<int32_t>(v);
            else std::cout << std::get<std::string>(v);
        }
        std::cout << "\n";
    }
}

// ---------- main ----------
int main() {
    std::cout << "MiniDB CLI (type \\q to quit)\n";
    if (!fs::exists("data")) {
        fs::create_directories("data");
        std::cout << "data�Ѵ���\n";
    }
    else {
        std::cout << "data�Ѵ���\n";
    }

    // ��ǰ������Ĭ�ϣ��޿⣻��Ҳ����Ĭ�Ͻ�һ�� "default"��
    std::string current_db;

    // �⼸������Ҫ�����ؽ��������°󶨡�
    CatalogManager cmgr("");   // ���� bind_db ������·��
    Catalog        catalog;
    std::unique_ptr<FileManager>   fm;
    std::unique_ptr<StorageEngine> storage;
    std::unique_ptr<Executor>      exec;

    auto bind_db = [&](const std::string& db)->bool {
        // 1) ȷ�� data/<db>/ ����
        if (!ensure_db_dir(db)) {
            std::cerr << "ERROR: create dir data/" << db << " failed.\n";
            return false;
        }

        // 1) �ؽ� cmgr / catalog
        cmgr = CatalogManager((fs::path("data") / db / "catalog.txt").string());
        catalog = Catalog();
        cmgr.LoadCatalog(catalog);
        
        // 2) �ؽ� FM / Storage / Executor��ע��˳�������ð�
        fm = std::make_unique<FileManager>((fs::path("data") / db).string(),
                                                           /*cache*/64, ReplacePolicy::LRU);
        storage = std::make_unique<StorageEngine>(cmgr, catalog, *fm);
        exec = std::make_unique<Executor>(cmgr, catalog, *storage);

        current_db = db;
        std::cout << "Database changed to " << current_db << ".\n";
        return true;
        };

    auto show_databases = [&]() {
        auto dbs = list_databases();
        std::cout << "+------------------+\n";
        std::cout << "| Databases        |\n";
        std::cout << "+------------------+\n";
        for (auto& d : dbs) std::cout << "| " << d << "\n";
        std::cout << "+------------------+\n";
        };

    auto create_database = [&](const std::string& db)->bool {
        if (db.empty()) { std::cerr << "CREATE DATABASE: name required\n"; return false; }
        fs::path p = fs::path("data") / db;
        if (fs::exists(p)) { std::cerr << "Database already exists: " << db << "\n"; return false; }
        if (!ensure_db_dir(db)) return false;
        // ��ʼ��һ���� catalog.txt����ѡ��
        CatalogManager temp((p / "catalog.txt").string());
        Catalog empty;
        temp.SaveCatalog(empty);
        std::cout << "Database '" << db << "' created.\n";
        return true;
        };

    auto drop_database = [&](const std::string& db)->bool {
        if (db.empty()) { std::cerr << "DROP DATABASE: name required\n"; return false; }
        if (current_db == db) {
            std::cerr << "Cannot drop the database in use: " << db << "\n";
            return false;
        }
        fs::path p = fs::path("data") / db;
        if (!fs::exists(p)) {
            std::cerr << "Database does not exist: " << db << "\n";
            return false;
        }
        std::error_code ec;
        fs::remove_all(p, ec);
        if (ec) { std::cerr << "Failed to drop database: " << ec.message() << "\n"; return false; }
        std::cout << "Database '" << db << "' dropped.\n";
        return true;
        };

    // ���� REPL
    std::string line, sql;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "\\q" || line == "quit" || line == "exit") break;

        // ���м����ĩβ���Դ� ;��
        std::string raw = trim_copy(line);
        if (raw.empty()) continue;

        // ���� �����ض�����������߱�����������
        // �����Сд��д�������зֺſ�ʡ
        {
            std::string up = upper_copy(raw);
            // SHOW DATABASES;
            if (up.rfind("SHOW DATABASES", 0) == 0) {
                show_databases();
                continue;
            }
            // CREATE DATABASE <name>;
            if (up.rfind("CREATE DATABASE", 0) == 0) {
                // ȡ����
                std::string db = trim_copy(raw.substr(std::string("CREATE DATABASE").size()));
                if (!db.empty() && db.back() == ';') db.pop_back();
                db = trim_copy(db);
                create_database(db);
                continue;
            }
            // DROP DATABASE <name>;
            if (up.rfind("DROP DATABASE", 0) == 0) {
                std::string db = trim_copy(raw.substr(std::string("DROP DATABASE").size()));
                if (!db.empty() && db.back() == ';') db.pop_back();
                db = trim_copy(db);
                drop_database(db);
                continue;
            }
            // USE <name>;
            if (up.rfind("USE ", 0) == 0) {
                std::string db = trim_copy(raw.substr(4));
                if (!db.empty() && db.back() == ';') db.pop_back();
                db = trim_copy(db);
                if (!fs::exists(fs::path("data") / db)) {
                    std::cerr << "Unknown database: " << db << "\n";
                }
                else {
                    bind_db(db);
                }
                continue;
            }
            // SHOW TABLES; / DESC ... / SHOW CREATE TABLE ... / ALTER TABLE ...
            // ��ЩĿǰ parser/planner ��δʵ�֣�ͳһ�ߡ���дִ������

        }

        // ���� �ۺ϶��� SQL��ֱ������ ';' ����������� ����
        sql += raw + "\n";
        // �����ǰ�����ﻹû�� ';'����������һ��
        if (!ends_with_semicolon(sql)) continue;

        // С���ߣ��ж��Ƿ� INSERT
        auto is_insert_stmt = [](const std::string& s)->bool {
            size_t i = 0; while (i < s.size() && isspace((unsigned char)s[i])) ++i;
            std::string head;
            while (i < s.size() && isalpha((unsigned char)s[i])) head.push_back((char)toupper((unsigned char)s[i++]));
            return head == "INSERT";
        };

        // �������ִ������������д��DDL/�����﷨�������������߱�������INSERT/SELECT/DELETE�ȣ�
        auto process_one_stmt = [&](const std::string& one_stmt)->bool {
            bool handled = false;
            bool ok = false;

            if (exec) {
                handled = true;
                ok = exec->Execute(one_stmt);   // ��дִ��������
                if (!ok) {
                    // ��дû�ɹ� -> �����߱�����
                    handled = false;
                }
            }

            if (!handled) {
                Status st = Status::OK();
                Lexer lx(one_stmt);
                Parser ps(lx);
                auto stmt = ps.parse_statement(st);

                if (!st.ok || !stmt) {
                    std::cerr << "Syntax error: " << st.message << "\n";
                    ok = false;
                }
                else {
                    CatalogEngineAdapter icat(cmgr, catalog);
                    SemanticAnalyzer sem(icat);
                    auto sres = sem.analyze(stmt.get());
                    if (!sres.status.ok) {
                        std::cerr << "Semantic error: " << sres.status.message << "\n";
                        ok = false;
                    }
                    else {
                        Planner pl;
                        Plan plan = pl.plan_from_stmt(stmt.get());
                        ok = (exec && exec->ExecutePlan(plan));
                        if (!ok) std::cerr << "Execution failed.\n";
                    }
                }
            }

            // ÿ����䶼�־û�һ��Ŀ¼
            cmgr.SaveCatalog(catalog);
            return ok;
            };

        // �ѻ������еĶ�����������г���ִ�У�����һ�� ';' ѭ����
        int inserted_in_batch = 0;
        while (true) {
            size_t pos = sql.find(';');
            if (pos == std::string::npos) break;

            std::string one_no_semi = trim_copy(sql.substr(0, pos));
            // ɾ���Ѵ����֣����� ';'��
            sql.erase(0, pos + 1);
            while (!sql.empty() && isspace((unsigned char)sql.front())) sql.erase(sql.begin());

            if (one_no_semi.empty()) continue;

            const std::string one_stmt = one_no_semi + ";";
            const bool is_ins = is_insert_stmt(one_stmt);
            bool ok = process_one_stmt(one_stmt);
            if (ok && is_ins) ++inserted_in_batch;
        }

        // ���������� INSERT���������
        if (inserted_in_batch == 1) {
            std::cout << "1 row inserted.\n";
        }
        else if (inserted_in_batch > 1) {
            std::cout << inserted_in_batch << " rows inserted.\n";
        }

        // ������а�䣨β��û�� ';' �Ĳ�������������һ�ֶ�ȡƴ��
        continue;
    }

    return 0;
}

// Ŀ¼�ṹ��
// database_system/
// ������ sql_compiler/
// ��   ������ ast.h �﷨��������
// ��   ������ lexer.h �ʷ�����
// ��   ������ lexer.cpp �ʷ�����
// ��   ������ parser.h �﷨����
// ��   ������ parser.cpp �﷨����
// ��   ������ semantic.h �������
// ��   ������ semantic.cpp �������
// ��   ������ pretty.h �����Ľ�
// ��   ������ pretty.cpp �����Ľ�
// ��   ������ catalog_iface.h ����Ŀ¼�ӿ�
// ��   ������ catalog_adapter_engine.h Ŀ¼����������
// ��   ������ planner.h ִ�мƻ�������
// ��   ������ planner.cpp ִ�мƻ�������
// ������ storage/
// ��   ������ file_manager.hpp����FileManager�ࣨ�ļ���ʼ����Ԫ���ݶ�д��ͳһ�洢�ӿڡ�ģ��Эͬ��
// ��   ������ file_manager.cppʵ��FileManager������г�Ա�������ļ�������Ԫ���ݳ־û����ӿڷ�װ��
// ��   ������ cache_manager.hpp����CacheManager�ࣨ����ṹ��LRU/FIFO ���ԡ�����ͳ�ơ����Ľӿڣ�
// ��   ������ cache_manager.cppʵ��CacheManager������г�Ա����������������滻�߼�����־�����
// ��   ������ page_manager.hpp����PageManager�ࣨҳ���� / �ͷš����̶�д�ӿڡ�����ҳ����
// ��   ������ page_manager.cppʵ��PageManager������г�Ա����������ҵ���߼���
// ��   ������ page.hpp����Page�ࣨҳ�ṹ��Ԫ��Ϣ���ʡ����ݶ�д�����л� / �����л���
// ��   ������ page.cppʵ��Page��ķ�������Ա��������serialize��deserialize��
// ������ engine/
// ��   ������ catalog_manager.hpp Ԫ���ݹ��������������ݿ��ṹ������Ϣ��������Ԫ����
// ��   ������ catalog_manager.cpp Ԫ���ݹ��������������ݿ��ṹ������Ϣ��������Ԫ����
// ��   ������ storage_engine.hpp �洢����ӿڣ�ʵ�ֶ������ļ��Ĳ��롢ɾ������ѯ�Ȳ�����
// ��   ������ storage_engine.cpp�洢����ӿڣ�ʵ�ֶ������ļ��Ĳ��롢ɾ������ѯ�Ȳ�����
// ��   ������ executor.hppִ����������ִ�мƻ����ô洢�����Ŀ¼��������� SQL ִ�С� 
// ��   ������ executor.cppִ����������ִ�мƻ����ô洢�����Ŀ¼��������� SQL ִ�С�
// ������ utils/
// ��   ������ common.hpp
// ��   ������ constants.hpp
// ��   ������ helpers.hpp
// ������ cli/
// ��   ������ main.cpp
// ������ tests/
//     ������ test_sql.cpp
//     ������ test_storage.cpp
//     ������ test_db.cpp