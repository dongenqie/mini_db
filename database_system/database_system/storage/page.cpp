#include "page.hpp"
#include <stdexcept>

using namespace std;

// 序列化：将页头元信息（页号、空闲偏移、上下页号）写入data数组前16字节
void Page::serialize() {
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
        throw invalid_argument("Page deserialize failed: disk data is null");
    }
    // 先将磁盘数据拷贝到当前页的data数组
    memcpy(data, disk_data, PAGE_SIZE);
    // 从data数组解析页头元信息
    uint32_t* header_ptr = reinterpret_cast<uint32_t*>(data);
    page_id = *header_ptr++;
    free_offset = *header_ptr++;
    prev_page_id = *header_ptr++;
    next_page_id = *header_ptr;
}