// =============================================
// cli/main.cpp  —
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

#include "../storage/file_manager.hpp"   // 新增
#include "../storage/cache_manager.hpp"  // 用到 ReplacePolicy 枚举
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

    // 1) 目录与目录持久化
    CatalogManager cmgr("data/catalog.txt");
    Catalog catalog;
    cmgr.LoadCatalog(catalog);

    // 2) 底层存储：构造 FileManager
    FileManager fm(
        "data",                 // 数据目录
        /*cache_cap*/ 64,       // 缓存页容量
        ReplacePolicy::LRU      // 或 FIFO
    );

    // 关键修正：把 catalog 也传进去，使用已存在的有效构造函数
    StorageEngine storage(cmgr, catalog, fm);

    Executor exec(cmgr, catalog, storage);

    std::string line, sql;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "\\q" || line == "quit" || line == "exit") break;

        sql += line + "\n";
        if (line.find(';') == std::string::npos) continue; // 读到 ';' 再执行

        // 2) 词法 + 语法
        Status st = Status::OK();
        Lexer lx(sql);
        Parser ps(lx);
        auto stmt = ps.parse_statement(st);
        if (!st.ok || !stmt) {
            std::cerr << "Syntax error: " << st.message << "\n";
            sql.clear();
            continue;
        }

        // 2) 语义（关键：用编译器提供的适配层，而不是把 CatalogManager 直接塞进来）
        minidb::CatalogEngineAdapter icat(cmgr, catalog);
        minidb::SemanticAnalyzer sem(icat);
        auto sres = sem.analyze(stmt.get());
        if (!sres.status.ok) {
            std::cerr << "Semantic error: " << sres.status.message << "\n";
            sql.clear();
            continue;
        }

        // 4) 计划
        Planner pl;
        Plan plan = pl.plan_from_stmt(stmt.get());

        // 4) 执行（用你项目里的接口名）
        if (!exec.ExecutePlan(plan)) {
            std::cerr << "Execution failed.\n";
        }

        cmgr.SaveCatalog(catalog); // 持久化目录
        sql.clear();
    }
    return 0;
}

// 目录结构：
// database_system/
// ├── sql_compiler/
// │   ├── ast.h 语法分析辅助
// │   ├── lexer.h 词法分析
// │   ├── lexer.cpp 词法分析
// │   ├── parser.h 语法分析
// │   ├── parser.cpp 语法分析
// │   ├── semantic.h 语义分析
// │   ├── semantic.cpp 语义分析
// │   ├── pretty.h 分析改进
// │   ├── pretty.cpp 分析改进
// │   ├── catalog_iface.h 抽象目录接口
// │   ├── catalog_adapter_engine.h 目录引擎适配器
// │   ├── planner.h 执行计划生成器
// │   └── planner.cpp 执行计划生成器
// ├── storage/
// │   ├── file_manager.hpp定义FileManager类（文件初始化、元数据读写、统一存储接口、模块协同）
// │   ├── file_manager.cpp实现FileManager类的所有成员函数（文件创建、元数据持久化、接口封装）
// │   ├── cache_manager.hpp定义CacheManager类（缓存结构、LRU/FIFO 策略、命中统计、核心接口）
// │   ├── cache_manager.cpp实现CacheManager类的所有成员函数（缓存操作、替换逻辑、日志输出）
// │   ├── page_manager.hpp定义PageManager类（页分配 / 释放、磁盘读写接口、空闲页管理）
// │   ├── page_manager.cpp实现PageManager类的所有成员函数（核心业务逻辑）
// │   ├── page.hpp定义Page类（页结构、元信息访问、数据读写、序列化 / 反序列化）
// │   └── page.cpp实现Page类的非内联成员函数（如serialize、deserialize）
// ├── engine/
// │   ├── catalog_manager.hpp 元数据管理器，管理数据库表结构、列信息、索引等元数据
// │   ├── catalog_manager.cpp 元数据管理器，管理数据库表结构、列信息、索引等元数据
// │   ├── storage_engine.hpp 存储引擎接口，实现对数据文件的插入、删除、查询等操作。
// │   ├── storage_engine.cpp存储引擎接口，实现对数据文件的插入、删除、查询等操作。
// │   ├── executor.hpp执行器，根据执行计划调用存储引擎和目录管理器完成 SQL 执行。 
// │   └── executor.cpp执行器，根据执行计划调用存储引擎和目录管理器完成 SQL 执行。
// ├── utils/
// │   ├── common.hpp
// │   ├── constants.hpp
// │   └── helpers.hpp
// ├── cli/
// │   └── main.cpp
// └── tests/
//     ├── test_sql.cpp
//     ├── test_storage.cpp
//     └── test_db.cpp