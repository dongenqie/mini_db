// =============================================
// storage/page.cpp
// =============================================
#include "page.hpp"
#include <stdexcept>
#include <cstring>

using namespace std;

#pragma pack(push,1)
struct PageHeaderPack {
    uint32_t page_id;
    uint32_t free_offset;
    uint32_t prev_page_id;
    uint32_t next_page_id;
};
#pragma pack(pop)

// 序列化：将页头元信息（页号、空闲偏移、上下页号）写入data数组前16字节
void Page::serialize() {
    PageHeaderPack h{ page_id, free_offset, prev_page_id, next_page_id };
    std::memcpy(data, &h, sizeof(h));
    // 将data数组首地址转为uint32_t指针，按顺序写入元信息
    uint32_t* header_ptr = reinterpret_cast<uint32_t*>(data);
    *header_ptr++ = page_id;       // 第1-4字节：页号
    *header_ptr++ = free_offset;   // 第5-8字节：空闲偏移
    *header_ptr++ = prev_page_id;  // 第9-12字节：上一页号
    *header_ptr = next_page_id;    // 第13-16字节：下一页号
}

// 反序列化：从磁盘读取的字节流（disk_data）解析页头，更新当前Page对象
void Page::deserialize(const char* disk_data) {
    if (disk_data == nullptr) {
        throw invalid_argument("page.cpp——反序列化失败: 字节流为空");
    }
    // 先将磁盘数据拷贝到当前页的data数组
    memcpy(data, disk_data, PAGE_SIZE);

    // 再把页头解包到成员变量
    PageHeaderPack h{};
    std::memcpy(&h, data, sizeof(h));
    page_id = h.page_id;
    free_offset = h.free_offset;
    prev_page_id = h.prev_page_id;
    next_page_id = h.next_page_id;
}