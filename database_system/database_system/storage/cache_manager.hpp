//定义CacheManager类（缓存结构、LRU/FIFO 策略、命中统计、核心接口）
#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "page_manager.hpp"
#include "page.hpp"
#include <unordered_map>
#include <list>
#include <string>
#include <time.h>
#include <fstream>
#include <stdexcept>

// 缓存替换策略（指导书要求支持LRU、FIFO）
enum class ReplacePolicy {
    LRU,    // 最近最少使用
    FIFO    // 先进先出
};

// 缓存页节点：存储页的缓存信息
struct CacheNode {
    Page page;          // 缓存的页对象
    time_t access_time; // 最后访问时间（LRU用）
    bool is_dirty;      // 脏页标记（修改后未刷盘）
    bool is_new;        // 加入顺序标记（FIFO用，true=先加入）

    CacheNode(const Page& p)
        : page(p), access_time(time(nullptr)),
        is_dirty(false), is_new(true) {}
};

class CacheManager {
private:
    PageManager& page_manager;          // 关联的页管理器（用于缓存未命中时读磁盘、脏页刷盘）
    unordered_map<uint32_t, CacheNode> cache_map; // 缓存哈希表（键：页号，值：缓存节点）
    uint32_t cache_capacity;            // 缓存最大容量（页数量，指导书未指定，用户可配置）
    ReplacePolicy policy;               // 缓存替换策略（LRU/FIFO）
    // 统计信息（指导书要求的缓存命中统计）
    uint32_t hit_count;                 // 命中次数
    uint32_t miss_count;                // 未命中次数
    string log_file_path;               // 日志文件路径（指导书要求的替换日志输出）

    // -------------------------- 替换策略核心函数（私有，内部调用） --------------------------
    // LRU策略：移除“最后访问时间最早”的缓存页
    void lru_evict();
    // FIFO策略：移除“最早加入缓存”的缓存页
    void fifo_evict();
    // 通用替换逻辑：根据策略调用对应evict函数
    void evict_page();
    // 写日志到文件（指导书要求的替换日志输出）
    void write_log(const string& log_content);

public:
    // 构造函数：初始化缓存管理器（关联页管理器、设置容量、策略、日志路径）
    CacheManager(PageManager& pm, uint32_t cap, ReplacePolicy pol, const string& log_path);

    // -------------------------- 指导书核心接口：获取页（get_page） --------------------------
    // 功能：先查缓存，命中则更新访问信息；未命中则读磁盘，加入缓存（满则替换）
    Page* get_page(uint32_t page_id);

    // -------------------------- 指导书核心接口：刷新页（flush_page） --------------------------
    // 功能：将缓存中的脏页写入磁盘，并标记为非脏页；页不在缓存则直接读磁盘刷新
    bool flush_page(uint32_t page_id);

    // -------------------------- 指导书扩展接口：刷新所有页（flush_all） --------------------------
    // 功能：刷新缓存中所有脏页到磁盘（程序退出时调用，确保数据持久化）
    void flush_all();

    // -------------------------- 指导书要求：缓存统计信息接口 --------------------------
    // 功能：获取命中次数、未命中次数、命中率
    void get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const;
    // 功能：输出统计信息到控制台（辅助调试）
    void print_stats() const;

    // -------------------------- 辅助接口（供测试/调试用） --------------------------
    // 获取缓存当前大小
    uint32_t get_current_size() const { return cache_map.size(); }
    // 获取缓存容量
    uint32_t get_capacity() const { return cache_capacity; }


    // 测试辅助接口：获取缓存哈希表（仅测试用）
    unordered_map<uint32_t, CacheNode>& get_cache_map() { return cache_map; }

    // 标记缓存页为脏页（供FileManager的write_page调用）
    void mark_dirty(uint32_t page_id) {
        auto it = cache_map.find(page_id);
        if (it != cache_map.end()) {
            it->second.is_dirty = true;
        }
    }
};

#endif // CACHE_MANAGER_H