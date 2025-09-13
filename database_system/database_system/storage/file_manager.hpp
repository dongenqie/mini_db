// =============================================
// storage/file_manager.hpp
// =============================================
//定义FileManager类（文件初始化、元数据读写、统一存储接口、模块协同）
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "page_manager.hpp"
#include "cache_manager.hpp"
#include <string>

// 数据库文件默认名称
#define DATA_FILE_NAME "data.dat"    // 数据文件（存储页数据）
#define META_FILE_NAME "meta.dat"    // 元数据文件（存储页管理元数据）

class FileManager {
private:
    std::string db_dir;              // 数据库文件根目录
    std::string data_file_path;      // 数据文件完整路径（db_dir + DATA_FILE_NAME）
    std::string meta_file_path;      // 元数据文件完整路径（db_dir + META_FILE_NAME）

    // 关联的底层模块
    PageManager page_manager;        // 页管理器（对接数据文件）
    CacheManager cache_manager;      // 缓存管理器（对接页管理器）

    // -------------------------- 元数据读写（私有：仅内部维护） --------------------------
    // 从meta.dat读取元数据（next_page_id、free_page_list），初始化PageManager
    void load_metadata();
    // 将PageManager的元数据（next_page_id、free_page_list）写入meta.dat，实现持久化
    void save_metadata();
    // 初始化数据库目录（若不存在则创建）
    void init_db_directory();

public:
    // 构造函数：初始化目录、文件路径、底层模块，加载元数据
    // 参数说明：db_dir-数据库目录，cache_cap-缓存容量，policy-缓存替换策略
    FileManager(const std::string& db_dir, uint32_t cache_cap, ReplacePolicy policy);

    // 析构函数：保存元数据，刷新所有缓存脏页（确保数据持久化，指导书核心要求）
    ~FileManager();

    // -------------------------- 指导书要求：统一存储接口（供数据库模块调用） --------------------------
    // 1. 分配页：调用PageManager分配页，返回页号
    uint32_t allocate_page();
    // 2. 释放页：调用PageManager释放页，将页号加入空闲列表
    bool free_page(uint32_t page_id);
    // 3. 读页：优先从缓存读，未命中则读文件，返回页指针（ nullptr表示失败）
    Page* read_page(uint32_t page_id);
    // 4. 写页：写入缓存并标记脏页（延迟刷盘，提升效率）
    bool write_page(uint32_t page_id, const Page& page);
    // 5. 刷新页：将指定页的缓存脏页写入文件，标记为非脏页
    bool flush_page(uint32_t page_id);
    // 6. 刷新所有：刷新缓存中所有脏页到文件（程序退出/事务提交时调用）
    void flush_all_pages();

    // -------------------------- 辅助接口（供测试/调试用） --------------------------
    // 获取缓存统计信息（命中次数、未命中次数、命中率）
    void get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const;
    // 打印缓存统计信息到控制台
    void print_cache_stats() const;
    // 获取数据文件路径
    std::string get_data_file_path() const { return data_file_path; }
    // 获取元数据文件路径
    std::string get_meta_file_path() const { return meta_file_path; }

    // 测试辅助接口：获取PageManager引用（仅测试用）
    const PageManager& get_page_manager() const { return page_manager; }
};



#endif // FILE_MANAGER_H
