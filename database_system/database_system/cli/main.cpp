// =============================================
// cli/main.cpp  ��
// =============================================
// cli/main.cpp
#include <iostream>
#include <string>

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
//#include "../engine/storage_engine.hpp"
#include "../engine/executor.hpp"

#include "../storage/file_manager.hpp"   // ����
#include "../storage/cache_manager.hpp"  // �õ� ReplacePolicy ö��
#include "../engine/storage_engine.hpp"

using namespace minidb;

static void print_rows(const std::vector<Row>& rows, const TableDef* opt_td) {
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

int main() {
    std::cout << "MiniDB CLI (type \\q to quit)\n";

    // 1) Ŀ¼��Ŀ¼�־û�
    CatalogManager cmgr("data/catalog.txt");
    Catalog catalog;
    cmgr.LoadCatalog(catalog);

    // 2) �ײ�洢������ FileManager
    FileManager fm(
        "data",                 // ����Ŀ¼
        /*cache_cap*/ 64,       // ����ҳ����
        ReplacePolicy::LRU      // �� FIFO
    );

    // �ؼ��������� catalog Ҳ����ȥ��ʹ���Ѵ��ڵ���Ч���캯��
    StorageEngine storage(cmgr, catalog, fm);

    Executor exec(cmgr, catalog, storage);

    std::string line, sql;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "\\q" || line == "quit" || line == "exit") break;

        sql += line + "\n";
        if (line.find(';') == std::string::npos) continue; // ���� ';' ��ִ��

        // 2) �ʷ� + �﷨
        Status st = Status::OK();
        Lexer lx(sql);
        Parser ps(lx);
        auto stmt = ps.parse_statement(st);
        if (!st.ok || !stmt) {
            std::cerr << "Syntax error: " << st.message << "\n";
            sql.clear();
            continue;
        }

        // 2) ���壨�ؼ����ñ������ṩ������㣬�����ǰ� CatalogManager ֱ����������
        minidb::CatalogEngineAdapter icat(cmgr, catalog);
        minidb::SemanticAnalyzer sem(icat);
        auto sres = sem.analyze(stmt.get());
        if (!sres.status.ok) {
            std::cerr << "Semantic error: " << sres.status.message << "\n";
            sql.clear();
            continue;
        }

        // 4) �ƻ�
        Planner pl;
        Plan plan = pl.plan_from_stmt(stmt.get());

        // 4) ִ�У�������Ŀ��Ľӿ�����
        if (!exec.ExecutePlan(plan)) {
            std::cerr << "Execution failed.\n";
        }

        cmgr.SaveCatalog(catalog); // �־û�Ŀ¼
        sql.clear();
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