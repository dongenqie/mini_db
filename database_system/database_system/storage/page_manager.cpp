// =============================================
// storage/page_manager.cpp
// =============================================
#include "page_manager.hpp"
#include <algorithm>
#include <iostream>

using namespace std;
// 构造函数：初始化数据文件和核心参数
PageManager::PageManager(const string& data_path)
    : data_file_path(data_path), next_page_id(1) {
    init_data_file(); // 确保数据文件存在
}

// 页分配：复用空闲页或创建新页
uint32_t PageManager::allocate_page() {
    uint32_t page_id;
    if (!free_page_list.empty()) {
        page_id = free_page_list.front();
        free_page_list.pop_front();
    }
    else {
        page_id = next_page_id++;
        // 预扩展文件：在正确偏移写一页零
        std::fstream fs(data_file_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) {
            // 文件可能还没创建为可读写，用 ofstream 先创建再打开
            std::ofstream create(data_file_path, std::ios::binary | std::ios::app);
            create.close();
            fs.open(data_file_path, std::ios::in | std::ios::out | std::ios::binary);
        }
        fs.seekp(get_page_offset(page_id), std::ios::beg);
        char zero[PAGE_SIZE] = { 0 };
        fs.write(zero, PAGE_SIZE);
        fs.close();
    }

    Page new_page(page_id);
    new_page.serialize();
    if (!write_page(page_id, new_page)) {
        throw std::runtime_error("allocate_page(): write_page failed");
    }
    return page_id;
}


// 页释放：将页号加入空闲列表（逻辑释放）
bool PageManager::free_page(uint32_t page_id) {
    // 合法性检查：页号无效或已在空闲列表中
    if (page_id == INVALID_PAGE_ID) {
        return false;
    }
    auto it = find(free_page_list.begin(), free_page_list.end(), page_id);
    if (it != free_page_list.end()) {
        return false; // 页已空闲，无需重复释放
    }

    // 清空页数据（避免敏感信息残留，提高数据安全性）
    Page empty_page(page_id);
    empty_page.serialize();
    if (!write_page(page_id, empty_page)) {
        return false;
    }

    // 将页号加入空闲列表
    free_page_list.push_back(page_id);
    return true;
}

// 页读取：从磁盘读取指定页号的内容到Page对象
bool PageManager::read_page(uint32_t page_id, Page& page) {
    if (page_id == INVALID_PAGE_ID) {
        return false;
    }
    uint64_t offset = get_page_offset(page_id);

    std::ifstream data_file(data_file_path, std::ios::in | std::ios::binary);
    if (!data_file) {
        return false;
    }

    // 检查偏移是否越界
    data_file.seekg(0, std::ios::end);
    std::streamoff file_size = data_file.tellg();
    if (file_size < 0 || offset + PAGE_SIZE > static_cast<uint64_t>(file_size)) {
        data_file.close();
        return false;
    }

    // 读取一页
    data_file.seekg(offset, std::ios::beg);
    char disk_page[PAGE_SIZE];
    data_file.read(disk_page, PAGE_SIZE);

    // 在关闭前检查读取字节数
    if (data_file.gcount() != PAGE_SIZE) {
        data_file.close();
        return false;
    }
    data_file.close();

    try {
        page.deserialize(disk_page);
    }
    catch (...) {
        return false;
    }
    return true;
}


// 页写入：将Page对象的数据写入磁盘对应页位置
bool PageManager::write_page(uint32_t page_id, const Page& page) {
    // 合法性检查：页号不匹配或无效
    if (page_id == INVALID_PAGE_ID || page.get_page_id() != page_id) {
        return false;
    }
    uint64_t offset = get_page_offset(page_id);

    // 打开数据文件（读写模式）并定位到页偏移
    fstream data_file(data_file_path, ios::in | ios::out | ios::binary);
    if (!data_file) {
        return false;
    }
    data_file.seekp(offset, ios::beg);
    if (!data_file) { // 定位失败（如文件损坏）
        data_file.close();
        return false;
    }

    // 序列化Page对象并写入磁盘
    Page temp_page = page;
    temp_page.serialize();
    data_file.write(temp_page.data, PAGE_SIZE);
    data_file.close();

    return true;
}