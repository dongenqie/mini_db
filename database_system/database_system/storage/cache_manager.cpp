// =============================================
// storage/cache_manager.cpp
// =============================================
//ʵ��CacheManager������г�Ա����������������滻�߼�����־�����
#include "cache_manager.hpp"
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <sstream>
#include<iostream>

using namespace std;

// -------------------------- ˽�и���������д��־ --------------------------
void CacheManager::write_log(const string& log_content) {
    // ����־�ļ���׷��ģʽ����д�롰ʱ��+���ݡ�
    ofstream log_file(log_file_path, ios::app);
    if (!log_file) {
        cerr << "Cache log write failed: cannot open log file - " << log_file_path << endl;
        return;
    }
    // ��ȡ��ǰʱ�䣨��ʽ��YYYY-MM-DD HH:MM:SS��

    time_t now = time(nullptr);
    tm local_tm;
    localtime_s(&local_tm, &now);

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "[%Y-%m-%d %H:%M:%S]", &local_tm);
    // д����־
    log_file << time_buf << " " << log_content << endl;
    log_file.close();
}

// -------------------------- ˽���滻���ԣ�LRU�Ƴ� --------------------------
void CacheManager::lru_evict() {
    if (cache_map.empty()) return;

    // �������棬�ҵ���������ʱ�����硱��ҳ
    auto least_recent_it = cache_map.begin();
    for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
        if (it->second.access_time < least_recent_it->second.access_time) {
            least_recent_it = it;
        }
    }

    // ��Ϊ��ҳ����ˢ��
    uint32_t evict_page_id = least_recent_it->first;
    if (least_recent_it->second.is_dirty) {
        if (page_manager.write_page(evict_page_id, least_recent_it->second.page)) {
            write_log("LRU evict: dirty page " + to_string(evict_page_id) + " flushed to disk");
        }
        else {
            write_log("LRU evict warning: dirty page " + to_string(evict_page_id) + " flush failed");
        }
    }

    // �Ƴ�����ҳ����¼��־
    write_log("LRU evict: page " + to_string(evict_page_id) + " removed from cache");
    cache_map.erase(least_recent_it);
}

// -------------------------- ˽���滻���ԣ�FIFO�Ƴ� --------------------------
void CacheManager::fifo_evict() {
    if (cache_map.empty()) return;

    // �������棬�ҵ���������롱��ҳ��is_new=false��ʾ�ȼ��룩
    auto oldest_it = cache_map.begin();
    for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
        if (!it->second.is_new) {
            oldest_it = it;
            break; // FIFOֻ���ҵ�һ��is_new=false��ҳ
        }
    }

    // ��Ϊ��ҳ����ˢ��
    uint32_t evict_page_id = oldest_it->first;
    if (oldest_it->second.is_dirty) {
        if (page_manager.write_page(evict_page_id, oldest_it->second.page)) {
            write_log("FIFO evict: dirty page " + to_string(evict_page_id) + " flushed to disk");
        }
        else {
            write_log("FIFO evict warning: dirty page " + to_string(evict_page_id) + " flush failed");
        }
    }

    // �Ƴ�����ҳ����¼��־
    write_log("FIFO evict: page " + to_string(evict_page_id) + " removed from cache");
    cache_map.erase(oldest_it);

    // ����ʣ�໺��ҳ��is_new��ǣ��¼���ı��Ϊtrue������Ϊfalse��
    for (auto& entry : cache_map) {
        entry.second.is_new = false;
    }
}

// -------------------------- ˽��ͨ���滻�߼� --------------------------
void CacheManager::evict_page() {
    if (policy == ReplacePolicy::LRU) {
        lru_evict();
    }
    else if (policy == ReplacePolicy::FIFO) {
        fifo_evict();
    }
}

// -------------------------- ���캯�� --------------------------
CacheManager::CacheManager(PageManager& pm, uint32_t cap, ReplacePolicy pol, const string& log_path)
    : page_manager(pm), cache_capacity(cap), policy(pol),
    log_file_path(log_path), hit_count(0), miss_count(0) {
    // ��ʼ����־�ļ���д��������Ϣ��
    write_log("CacheManager initialized: capacity=" + to_string(cap) +
        ", policy=" + (policy == ReplacePolicy::LRU ? "LRU" : "FIFO"));
}

// -------------------------- ���Ľӿڣ�get_page --------------------------
Page* CacheManager::get_page(uint32_t page_id) {
    // 1. ��黺���Ƿ�����
    auto it = cache_map.find(page_id);
    if (it != cache_map.end()) {
        // ���У����·�����Ϣ��LRU����ʱ�䣬FIFO�޲�����
        hit_count++;
        if (policy == ReplacePolicy::LRU) {
            it->second.access_time = time(nullptr); // ����������ʱ��
        }
        // ��¼������־
        string log = "Cache hit: page " + to_string(page_id);
        write_log(log);
        return &(it->second.page); // ���ػ���ҳ��ָ��
    }

    // 2. ����δ���У��Ӵ��̶�ȡҳ
    miss_count++;
    Page disk_page(page_id);
    if (!page_manager.read_page(page_id, disk_page)) {
        // ���̶�ȡʧ�ܣ���ҳ����Ч��
        string log = "Cache miss: page " + to_string(page_id) + " read from disk failed";
        write_log(log);
        return nullptr;
    }
    write_log("Cache miss: page " + to_string(page_id) + " read from disk");

    // 3. ��������ִ���滻����
    if (cache_map.size() >= cache_capacity) {
        write_log("Cache full: trigger evict policy");
        evict_page();
    }

    // 4. ������ҳ���뻺�棨FIFO���Ϊ�¼��룩
    auto insert_it = cache_map.emplace(page_id, CacheNode(disk_page)).first;
    if (policy == ReplacePolicy::FIFO) {
        // �Ƚ����л���ҳ��is_new��Ϊfalse���ٱ�ǵ�ǰҳΪtrue
        for (auto& entry : cache_map) {
            entry.second.is_new = false;
        }
        insert_it->second.is_new = true;
    }
    write_log("Cache add: page " + to_string(page_id) + " added to cache");

    // 5. ���ػ���ҳָ��
    return &(insert_it->second.page);
}

// -------------------------- ���Ľӿڣ�flush_page --------------------------
bool CacheManager::flush_page(uint32_t page_id) {
    // 1. ���ҳ�Ƿ��ڻ�����
    auto it = cache_map.find(page_id);
    if (it != cache_map.end()) {
        // ҳ�ڻ����У���Ϊ��ҳ��д����̲����Ϊ����ҳ
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
            // ����ҳ������д��
            string log = "Flush page skipped: page " + to_string(page_id) + " is not dirty";
            write_log(log);
            return true;
        }
    }

    // 2. ҳ���ڻ����У�ֱ�ӴӴ��̶�ȡ��ˢ��,ʵ�ֽӿ�������
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

// -------------------------- ��չ�ӿڣ�flush_all --------------------------
void CacheManager::flush_all() {
    write_log("Flush all dirty pages start");
    uint32_t flush_success = 0;
    uint32_t flush_failed = 0;

    // �������л���ҳ��ˢ����ҳ
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

    // ��¼ˢ�½����־
    string log = "Flush all dirty pages end: success=" + to_string(flush_success) +
        ", failed=" + to_string(flush_failed);
    write_log(log);
}

// -------------------------- ����ͳ����Ϣ --------------------------
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