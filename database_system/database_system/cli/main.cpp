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
// │   ├── file_manager.hpp
// │   ├── file_manager.cpp
// │   ├── buffer.hpp
// │   ├── buffer.cpp
// │   ├── page.hpp
// │   └── page.cpp
// ├── engine/
// │   ├── catalog_manager.hpp
// │   ├── catalog_manager.cpp
// │   ├── storage_engine.hpp
// │   ├── storage_engine.cpp
// │   ├── executor.hpp
// │   └── executor.cpp
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