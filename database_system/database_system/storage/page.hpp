#ifndef PAGE_H
#define PAGE_H

#include <cstdint>
#include <cstring>
#include <string>

// 核心常量定义
#define PAGE_SIZE 4096          // 每页固定大小：4KB
#define PAGE_HEADER_SIZE 16     // 页头大小：16字节（存储4个uint32_t元信息）
#define INVALID_PAGE_ID 0       // 无效页号（页号从1开始）

class Page {
    // 声明PageManager为友元类，允许其访问所有私有成员
    friend class PageManager;
    friend class FileManager;

private:
    uint32_t page_id;          // 唯一页编号（≥1）
    uint32_t free_offset;      // 数据区空闲空间起始偏移（从PAGE_HEADER_SIZE开始）
    uint32_t prev_page_id;     // 上一页编号（用于数据表的页链表组织，实现数据表映射）
    uint32_t next_page_id;     // 下一页编号
    char data[PAGE_SIZE];      // 页完整数据（页头+数据区，共4KB）

public:
    // 构造函数：初始化页号，默认空闲偏移为页头后（数据区起始位置）
    Page(uint32_t pid = INVALID_PAGE_ID)
        : page_id(pid), free_offset(PAGE_HEADER_SIZE),
        prev_page_id(INVALID_PAGE_ID), next_page_id(INVALID_PAGE_ID) {
        memset(data, 0, PAGE_SIZE); // 初始化数据区为0，避免脏数据残留
    }

    // -------------------------- 页元信息访问接口（供PageManager调用） --------------------------
    uint32_t get_page_id() const { return page_id; }
    void set_page_id(uint32_t pid) { page_id = pid; }

    uint32_t get_free_offset() const { return free_offset; }

    // 更新空闲偏移（确保不超出页大小，避免数据溢出）
    bool set_free_offset(uint32_t offset) {
        if (offset < PAGE_HEADER_SIZE || offset > PAGE_SIZE) return false;
        free_offset = offset;
        return true;
    }

    uint32_t get_prev_page_id() const { return prev_page_id; }
    void set_prev_page_id(uint32_t pid) { prev_page_id = pid; }

    uint32_t get_next_page_id() const { return next_page_id; }
    void set_next_page_id(uint32_t pid) { next_page_id = pid; }

    // -------------------------- 页数据读写接口（供数据库存储引擎调用） --------------------------
    // 向数据区写入数据（从指定偏移开始，需确保不超出页大小）
    bool write_data(uint32_t offset, const char* data_buf, uint32_t len) {
        if (offset + len > PAGE_SIZE || data_buf == nullptr) return false;
        memcpy(data + offset, data_buf, len);
        return true;
    }

    // 从数据区读取数据（从指定偏移开始，需确保不超出页大小）
    bool read_data(uint32_t offset, char* data_buf, uint32_t len) const {
        if (offset + len > PAGE_SIZE || data_buf == nullptr) return false;
        memcpy(data_buf, data + offset, len);
        return true;
    }

    // -------------------------- 序列化/反序列化（页与磁盘文件的格式转换） --------------------------
    // 序列化：将页头元信息写入data数组（前16字节），用于写入磁盘
    void serialize();

    // 反序列化：从磁盘读取的字节流中解析页头元信息，赋值给当前Page对象
    void deserialize(const char* disk_data);
};

#endif // PAGE_H
