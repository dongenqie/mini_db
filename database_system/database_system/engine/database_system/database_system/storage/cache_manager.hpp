//����CacheManager�ࣨ����ṹ��LRU/FIFO ���ԡ�����ͳ�ơ����Ľӿڣ�
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

// �����滻���ԣ�ָ����Ҫ��֧��LRU��FIFO��
enum class ReplacePolicy {
    LRU,    // �������ʹ��
    FIFO    // �Ƚ��ȳ�
};

// ����ҳ�ڵ㣺�洢ҳ�Ļ�����Ϣ
struct CacheNode {
    Page page;          // �����ҳ����
    time_t access_time; // ������ʱ�䣨LRU�ã�
    bool is_dirty;      // ��ҳ��ǣ��޸ĺ�δˢ�̣�
    bool is_new;        // ����˳���ǣ�FIFO�ã�true=�ȼ��룩

    CacheNode(const Page& p)
        : page(p), access_time(time(nullptr)),
        is_dirty(false), is_new(true) {}
};

class CacheManager {
private:
    PageManager& page_manager;          // ������ҳ�����������ڻ���δ����ʱ�����̡���ҳˢ�̣�
    unordered_map<uint32_t, CacheNode> cache_map; // �����ϣ������ҳ�ţ�ֵ������ڵ㣩
    uint32_t cache_capacity;            // �������������ҳ������ָ����δָ�����û������ã�
    ReplacePolicy policy;               // �����滻���ԣ�LRU/FIFO��
    // ͳ����Ϣ��ָ����Ҫ��Ļ�������ͳ�ƣ�
    uint32_t hit_count;                 // ���д���
    uint32_t miss_count;                // δ���д���
    string log_file_path;               // ��־�ļ�·����ָ����Ҫ����滻��־�����

    // -------------------------- �滻���Ժ��ĺ�����˽�У��ڲ����ã� --------------------------
    // LRU���ԣ��Ƴ���������ʱ�����硱�Ļ���ҳ
    void lru_evict();
    // FIFO���ԣ��Ƴ���������뻺�桱�Ļ���ҳ
    void fifo_evict();
    // ͨ���滻�߼������ݲ��Ե��ö�Ӧevict����
    void evict_page();
    // д��־���ļ���ָ����Ҫ����滻��־�����
    void write_log(const string& log_content);

public:
    // ���캯������ʼ�����������������ҳ���������������������ԡ���־·����
    CacheManager(PageManager& pm, uint32_t cap, ReplacePolicy pol, const string& log_path);

    // -------------------------- ָ������Ľӿڣ���ȡҳ��get_page�� --------------------------
    // ���ܣ��Ȳ黺�棬��������·�����Ϣ��δ����������̣����뻺�棨�����滻��
    Page* get_page(uint32_t page_id);

    // -------------------------- ָ������Ľӿڣ�ˢ��ҳ��flush_page�� --------------------------
    // ���ܣ��������е���ҳд����̣������Ϊ����ҳ��ҳ���ڻ�����ֱ�Ӷ�����ˢ��
    bool flush_page(uint32_t page_id);

    // -------------------------- ָ������չ�ӿڣ�ˢ������ҳ��flush_all�� --------------------------
    // ���ܣ�ˢ�»�����������ҳ�����̣������˳�ʱ���ã�ȷ�����ݳ־û���
    void flush_all();

    // -------------------------- ָ����Ҫ�󣺻���ͳ����Ϣ�ӿ� --------------------------
    // ���ܣ���ȡ���д�����δ���д�����������
    void get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const;
    // ���ܣ����ͳ����Ϣ������̨���������ԣ�
    void print_stats() const;

    // -------------------------- �����ӿڣ�������/�����ã� --------------------------
    // ��ȡ���浱ǰ��С
    uint32_t get_current_size() const { return cache_map.size(); }
    // ��ȡ��������
    uint32_t get_capacity() const { return cache_capacity; }


    // ���Ը����ӿڣ���ȡ�����ϣ���������ã�
    unordered_map<uint32_t, CacheNode>& get_cache_map() { return cache_map; }

    // ��ǻ���ҳΪ��ҳ����FileManager��write_page���ã�
    void mark_dirty(uint32_t page_id) {
        auto it = cache_map.find(page_id);
        if (it != cache_map.end()) {
            it->second.is_dirty = true;
        }
    }
};

#endif // CACHE_MANAGER_H