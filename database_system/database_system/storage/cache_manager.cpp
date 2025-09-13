// =============================================
// storage/cache_manager.cpp
// =============================================
//实现CacheManager类的所有成员函数（缓存操作、替换逻辑、日志输出）
#include "cache_manager.hpp"
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <sstream>
#include<iostream>

using namespace std;

// -------------------------- 私有辅助函数：写日志 --------------------------
void CacheManager::write_log(const string& log_content) {
    // 打开日志文件（追加模式），写入“时间+内容”
    ofstream log_file(log_file_path, ios::app);
    if (!log_file) {
        cerr << "Cache log write failed: cannot open log file - " << log_file_path << endl;
        return;
    }
    // 获取当前时间（格式：YYYY-MM-DD HH:MM:SS）

    time_t now = time(nullptr);
    tm local_tm;
    localtime_s(&local_tm, &now);

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "[%Y-%m-%d %H:%M:%S]", &local_tm);
    // 写入日志
    log_file << time_buf << " " << log_content << endl;
    log_file.close();
}

// -------------------------- 私有替换策略：LRU移除 --------------------------
void CacheManager::lru_evict() {
    if (cache_map.empty()) return;

    // 遍历缓存，找到“最后访问时间最早”的页
    auto least_recent_it = cache_map.begin();
    for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
        if (it->second.access_time < least_recent_it->second.access_time) {
            least_recent_it = it;
        }
    }

    // 若为脏页，先刷盘
    uint32_t evict_page_id = least_recent_it->first;
    if (least_recent_it->second.is_dirty) {
        if (page_manager.write_page(evict_page_id, least_recent_it->second.page)) {
            write_log("LRU evict: dirty page " + to_string(evict_page_id) + " flushed to disk");
        }
        else {
            write_log("LRU evict warning: dirty page " + to_string(evict_page_id) + " flush failed");
        }
    }

    // 移除缓存页，记录日志
    write_log("LRU evict: page " + to_string(evict_page_id) + " removed from cache");
    cache_map.erase(least_recent_it);
}

// -------------------------- 私有替换策略：FIFO移除 --------------------------
void CacheManager::fifo_evict() {
    if (cache_map.empty()) return;

    // 遍历缓存，找到“最早加入”的页（is_new=false表示先加入）
    auto oldest_it = cache_map.begin();
    for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
        if (!it->second.is_new) {
            oldest_it = it;
            break; // FIFO只需找第一个is_new=false的页
        }
    }

    // 若为脏页，先刷盘
    uint32_t evict_page_id = oldest_it->first;
    if (oldest_it->second.is_dirty) {
        if (page_manager.write_page(evict_page_id, oldest_it->second.page)) {
            write_log("FIFO evict: dirty page " + to_string(evict_page_id) + " flushed to disk");
        }
        else {
            write_log("FIFO evict warning: dirty page " + to_string(evict_page_id) + " flush failed");
        }
    }

    // 移除缓存页，记录日志
    write_log("FIFO evict: page " + to_string(evict_page_id) + " removed from cache");
    cache_map.erase(oldest_it);

    // 更新剩余缓存页的is_new标记（新加入的标记为true，其余为false）
    for (auto& entry : cache_map) {
        entry.second.is_new = false;
    }
}

// -------------------------- 私有通用替换逻辑 --------------------------
void CacheManager::evict_page() {
    if (policy == ReplacePolicy::LRU) {
        lru_evict();
    }
    else if (policy == ReplacePolicy::FIFO) {
        fifo_evict();
    }
}

// -------------------------- 构造函数 --------------------------
CacheManager::CacheManager(PageManager& pm, uint32_t cap, ReplacePolicy pol, const string& log_path)
    : page_manager(pm), cache_capacity(cap), policy(pol),
    log_file_path(log_path), hit_count(0), miss_count(0) {
    // 初始化日志文件（写入启动信息）
    write_log("CacheManager initialized: capacity=" + to_string(cap) +
        ", policy=" + (policy == ReplacePolicy::LRU ? "LRU" : "FIFO"));
}

// -------------------------- 核心接口：get_page --------------------------
Page* CacheManager::get_page(uint32_t page_id) {
    // 1. 检查缓存是否命中
    auto it = cache_map.find(page_id);
    if (it != cache_map.end()) {
        // 命中：更新访问信息（LRU更新时间，FIFO无操作）
        hit_count++;
        if (policy == ReplacePolicy::LRU) {
            it->second.access_time = time(nullptr); // 更新最后访问时间
        }
        // 记录命中日志
        string log = "Cache hit: page " + to_string(page_id);
        write_log(log);
        return &(it->second.page); // 返回缓存页的指针
    }

    // 2. 缓存未命中：从磁盘读取页
    miss_count++;
    Page disk_page(page_id);
    if (!page_manager.read_page(page_id, disk_page)) {
        // 磁盘读取失败（如页号无效）
        string log = "Cache miss: page " + to_string(page_id) + " read from disk failed";
        write_log(log);
        return nullptr;
    }
    write_log("Cache miss: page " + to_string(page_id) + " read from disk");

    // 3. 缓存满：执行替换策略
    if (cache_map.size() >= cache_capacity) {
        write_log("Cache full: trigger evict policy");
        evict_page();
    }

    // 4. 将磁盘页加入缓存（FIFO标记为新加入）
    auto insert_it = cache_map.emplace(page_id, CacheNode(disk_page)).first;
    if (policy == ReplacePolicy::FIFO) {
        // 先将所有缓存页的is_new设为false，再标记当前页为true
        for (auto& entry : cache_map) {
            entry.second.is_new = false;
        }
        insert_it->second.is_new = true;
    }
    write_log("Cache add: page " + to_string(page_id) + " added to cache");

    // 5. 返回缓存页指针
    return &(insert_it->second.page);
}

// -------------------------- 核心接口：flush_page --------------------------
bool CacheManager::flush_page(uint32_t page_id) {
    // 1. 检查页是否在缓存中
    auto it = cache_map.find(page_id);
    if (it != cache_map.end()) {
        // 页在缓存中：若为脏页，写入磁盘并标记为非脏页
        if (it->second.is_dirty) {
            if (page_manager.write_page(page_id, it->second.page)) {
                it->second.is_dirty = false;
                string log = "Flush page success: page " + to_string(page_id) + " flushed to disk";
                write_log(log);
                return true;
            }
            else {
                string log = "Flush page failed: page " + to_string(page_id) + " flush to disk error";
                write_log(log);
                return false;
            }
        }
        else {
            // 非脏页，无需写入
            string log = "Flush page skipped: page " + to_string(page_id) + " is not dirty";
            write_log(log);
            return true;
        }
    }

    // 2. 页不在缓存中：直接从磁盘读取后刷新,实现接口完整性
    Page disk_page(page_id);
    if (page_manager.read_page(page_id, disk_page)) {
        if (page_manager.write_page(page_id, disk_page)) {
            string log = "Flush page success: page " + to_string(page_id) + " (not in cache) flushed to disk";
            write_log(log);
            return true;
        }
    }

    string log = "Flush page failed: page " + to_string(page_id) + " (not in cache) read/write error";
    write_log(log);
    return false;
}

// -------------------------- 扩展接口：flush_all --------------------------
void CacheManager::flush_all() {
    write_log("Flush all dirty pages start");
    uint32_t flush_success = 0;
    uint32_t flush_failed = 0;

    // 遍历所有缓存页，刷新脏页
    for (auto& entry : cache_map) {
        if (entry.second.is_dirty) {
            if (page_manager.write_page(entry.first, entry.second.page)) {
                entry.second.is_dirty = false;
                flush_success++;
            }
            else {
                flush_failed++;
            }
        }
    }

    // 记录刷新结果日志
    string log = "Flush all dirty pages end: success=" + to_string(flush_success) +
        ", failed=" + to_string(flush_failed);
    write_log(log);
}

// -------------------------- 缓存统计信息 --------------------------
void CacheManager::get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const {
    hit = hit_count;
    miss = miss_count;
    uint32_t total = hit + miss;
    hit_rate = (total == 0) ? 0.0 : static_cast<double>(hit) / total;
}

void CacheManager::print_stats() const {
    uint32_t hit, miss;
    double hit_rate;
    get_cache_stats(hit, miss, hit_rate);

    cout << "=== Cache Statistics ===" << endl;
    cout << "Total Access: " << hit + miss << endl;
    cout << "Hit Count:    " << hit << endl;
    cout << "Miss Count:   " << miss << endl;
    cout << "Hit Rate:     " << fixed << setprecision(2) << hit_rate * 100 << "%" << endl;
    cout << "=======================" << endl;
}