mini_db
第30组 董恩琪 阙文中 阮高千金
目录结构：
database_system/
├── sql_compiler/
│   ├── ast.h 语法分析辅助
│   ├── lexer.h 词法分析
│   ├── lexer.cpp 词法分析
│   ├── parser.h 语法分析
│   ├── parser.cpp 语法分析
│   ├── semantic.h 语义分析
│   ├── semantic.cpp 语义分析
│   ├── pretty.h 分析改进
│   ├── pretty.cpp 分析改进
│   ├── catalog_iface.h 抽象目录接口
│   ├── catalog_adapter_engine.h 目录引擎适配器
│   ├── planner.h 执行计划生成器
│   └── planner.cpp 执行计划生成器
├── storage/
│   ├── file_manager.hpp定义FileManager类（文件初始化、元数据读写、统一存储接口、模块协同）
│   ├── file_manager.cpp实现FileManager类的所有成员函数（文件创建、元数据持久化、接口封装）
│   ├── cache_manager.hpp定义CacheManager类（缓存结构、LRU/FIFO 策略、命中统计、核心接口）
│   ├── cache_manager.cpp实现CacheManager类的所有成员函数（缓存操作、替换逻辑、日志输出）
│   ├── page_manager.hpp定义PageManager类（页分配 / 释放、磁盘读写接口、空闲页管理）
│   ├── page_manager.cpp实现PageManager类的所有成员函数（核心业务逻辑）
│   ├── page.hpp定义Page类（页结构、元信息访问、数据读写、序列化 / 反序列化）
│   └── page.cpp实现Page类的非内联成员函数（如serialize、deserialize）
├── engine/
│   ├── catalog_manager.hpp 元数据管理器，管理数据库表结构、列信息、索引等元数据
│   ├── catalog_manager.cpp 元数据管理器，管理数据库表结构、列信息、索引等元数据
│   ├── storage_engine.hpp 存储引擎接口，实现对数据文件的插入、删除、查询等操作。
│   ├── storage_engine.cpp存储引擎接口，实现对数据文件的插入、删除、查询等操作。
│   ├── executor.hpp执行器，根据执行计划调用存储引擎和目录管理器完成 SQL 执行。 
│   └── executor.cpp执行器，根据执行计划调用存储引擎和目录管理器完成 SQL 执行。
├── utils/
│   ├── common.hpp
│   ├── constants.hpp
│   └── helpers.hpp
├── cli/
│   ├── linenoise.hpp 智能输入提示（支持命令行历史，自动补全）
│   ├── linenoise.cpp 智能输入提示（支持命令行历史，自动补全）
│   └── main.cpp 集成数据库系统入口
└── tests/
    ├── test_sql.cpp sql编译器单元测试文件
    ├── test_storage.cpp 存储系统单元测试文件
    └── test_db.cpp 数据库引擎单元测试文件

如何编译和运行？
本项目使用的语言100% C++17
①文件项目用编译器打开database_system.sln文件，推荐使用visual studio2022；
②将cli/、sql_compiler/、storage/、engine/、utils/文件夹->右键->包含在项目之中；
③将tests/文件夹->右键->从项目中排除；
④运行整个文件（ctrl+shift+B -> F5).
