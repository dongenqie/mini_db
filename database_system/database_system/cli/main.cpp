// =============================================
// cli/main.cpp  —
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

#include "../storage/file_manager.hpp"   // 新增
#include "../storage/cache_manager.hpp"  // 用到 ReplacePolicy 枚举

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

// 列出 data/ 下所有“数据库目录”（包含 catalog.txt 或非空目录）
static std::vector<std::string> list_databases(const std::string& root = "data") {
    std::vector<std::string> out;
    if (!fs::exists(root)) return out;
    for (auto& de : fs::directory_iterator(root)) {
        if (!de.is_directory()) continue;
        auto db = de.path().filename().string();
        // 至少有 catalog.txt 或者空目录也算库（你也可以限制必须有 catalog.txt）
        out.push_back(db);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// 确保 data/<db>/ 目录存在
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
        std::cout << "data已创建\n";
    }
    else {
        std::cout << "data已存在\n";
    }

    // 当前库名（默认：无库；你也可以默认建一个 "default"）
    std::string current_db;

    // 这几个对象要“可重建、可重新绑定”
    CatalogManager cmgr("");   // 将在 bind_db 中重设路径
    Catalog        catalog;
    std::unique_ptr<FileManager>   fm;
    std::unique_ptr<StorageEngine> storage;
    std::unique_ptr<Executor>      exec;

    auto bind_db = [&](const std::string& db)->bool {
        // 1) 确保 data/<db>/ 存在
        if (!ensure_db_dir(db)) {
            std::cerr << "ERROR: create dir data/" << db << " failed.\n";
            return false;
        }

        // 1) 重建 cmgr / catalog
        cmgr = CatalogManager((fs::path("data") / db / "catalog.txt").string());
        catalog = Catalog();
        cmgr.LoadCatalog(catalog);
        
        // 2) 重建 FM / Storage / Executor，注意顺序与引用绑定
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
        // 初始化一个空 catalog.txt（可选）
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

    // 进入 REPL
    std::string line, sql;
    while (true) {
        std::cout << "minidb> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "\\q" || line == "quit" || line == "exit") break;

        // 单行即命令（末尾可以带 ;）
        std::string raw = trim_copy(line);
        if (raw.empty()) continue;

        // —— 先拦截多库相关命令（不走编译器）——
        // 允许大小写混写；参数中分号可省
        {
            std::string up = upper_copy(raw);
            // SHOW DATABASES;
            if (up.rfind("SHOW DATABASES", 0) == 0) {
                show_databases();
                continue;
            }
            // CREATE DATABASE <name>;
            if (up.rfind("CREATE DATABASE", 0) == 0) {
                // 取库名
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
            // 这些目前 parser/planner 尚未实现，统一走“手写执行器”
            if (up.rfind("SHOW TABLES", 0) == 0 ||
                up.rfind("DESC ", 0) == 0 ||
                up.rfind("DESCRIBE ", 0) == 0 ||
                up.rfind("SHOW CREATE TABLE", 0) == 0 ||
                up.rfind("ALTER TABLE", 0) == 0) {
                if (exec) {
                    std::string stmt = raw;
                    if (!ends_with_semicolon(stmt)) stmt += ";";
                    if (!exec->Execute(stmt)) std::cerr << "Execution failed.\n";
                    cmgr.SaveCatalog(catalog);
                }
                continue;
            }
        }

        // —— 聚合多行 SQL，直到遇到 ';' 再送入编译器 ——
        sql += raw + "\n";
        if (!ends_with_semicolon(sql)) continue;

        // 词法 + 语法
        Status st = Status::OK();
        Lexer lx(sql);
        Parser ps(lx);
        auto stmt = ps.parse_statement(st);

        // 哪些 plan.op 我们已在 ExecutePlan 里实现
        auto op_supported_by_plan = [](const Plan& plan) -> bool {
            if (!plan.root) return false;
            using minidb::PlanOp;
            switch (plan.root->op) {
            case PlanOp::CREATE:
            case PlanOp::INSERT:
            case PlanOp::PROJECT:
            case PlanOp::FILTER:
            case PlanOp::SEQSCAN:
            case PlanOp::DELETE_:
            case PlanOp::DROP:
                return true;
            default:
                return false;
            }
        };

        bool ran = false;
        if (!st.ok || !stmt) {
            // 语法阶段就不认识 -> 回退手写执行器（可覆盖 CREATE/INSERT/SELECT/DELETE/DROP 以外的语句）
            if (exec && !exec->Execute(sql)) std::cerr << "Execution failed.\n";
            ran = true;
        }
        else {
            // 语义
            minidb::CatalogEngineAdapter icat(cmgr, catalog);
            minidb::SemanticAnalyzer sem(icat);
            auto sres = sem.analyze(stmt.get());
            if (!sres.status.ok) {
                // 语义不过 -> 回退
                if (exec && !exec->Execute(sql)) std::cerr << "Execution failed.\n";
                ran = true;
            }
            else {
                // 计划
                Planner pl;
                Plan plan = pl.plan_from_stmt(stmt.get());
                if (op_supported_by_plan(plan)) {
                    // 我们支持的那些 op 走计划执行
                    if (!exec || !exec->ExecutePlan(plan)) std::cerr << "Execution failed.\n";
                    ran = true;
                }
                else {
                    // 其它 op（如 SHOW/DESC/ALTER/USE）回退手写执行器
                    if (exec && !exec->Execute(sql)) std::cerr << "Execution failed.\n";
                    ran = true;
                }
            }
        }

        // 落盘当前库的目录（不管走哪条路径）
        cmgr.SaveCatalog(catalog);
        sql.clear();
        continue;
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