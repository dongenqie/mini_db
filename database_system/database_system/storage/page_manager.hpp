// =============================================
// storage/page_manager.hpp
// =============================================
#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "page.hpp"
#include <list>
#include <string>
#include <fstream>
#include <stdexcept>

using namespace std;

class PageManager {
private:
    string data_file_path;       // 物理数据文件路径
    uint32_t next_page_id;       // 下一个待分配的新页号（初始为1，确保页号唯一）
    list<uint32_t> free_page_list; // 空闲页列表（维护可复用的页号，避免磁盘碎片）

    // 关键修正：页偏移必须是 (page_id - 1) * PAGE_SIZE
    uint64_t get_page_offset(uint32_t page_id) const {
        // 页号从 1 开始
        return static_cast<uint64_t>(page_id - 1) * PAGE_SIZE;
    }

    // 辅助函数：初始化数据文件（若不存在则创建空文件，确保后续读写正常）
    void init_data_file() {
        ifstream file(data_file_path, ios::binary);
        if (!file) { // 文件不存在，创建二进制空文件
            ofstream new_file(data_file_path, ios::binary);
            if (!new_file) {
                throw runtime_error("page_manager.hpp——页管理初始化失败: 创建文件 " + data_file_path + "失败");
            }
            new_file.close();
        }
        else {
            file.close();
        }
    }

public:
 
    // 构造函数：初始化数据文件路径、空闲页列表、下一页号
    PageManager(const string& data_path);

    // -------------------------- 核心功能：页分配 --------------------------
    // 功能：从空闲页列表复用页，无空闲页则分配新页（返回页号）
    uint32_t allocate_page();

    // -------------------------- 核心功能：页释放 --------------------------
    // 功能：将页号加入空闲列表（逻辑释放，暂不物理删除）
    bool free_page(uint32_t page_id);

    // -------------------------- 核心接口：页读取（read_page） --------------------------
    // 功能：根据页号从磁盘读取页数据，存入输出参数page
    bool read_page(uint32_t page_id, Page& page);

    // -------------------------- 核心接口：页写入（write_page） --------------------------
    // 功能：将Page对象的数据写入磁盘对应页位置
    bool write_page(uint32_t page_id, const Page& page);

    // -------------------------- 辅助接口（供缓存/数据库模块调用） --------------------------
    // 获取空闲页列表（缓存模块判断页是否可复用）
    list<uint32_t> get_free_page_list() const { return free_page_list; }
    // 设置空闲页列表
    void set_free_page_list(const std::list<uint32_t>& free_list) {
        free_page_list = free_list;
    }


    // 获取下一页号（元数据持久化时使用）
    uint32_t get_next_page_id() const { return next_page_id; }
    // 设置下一个页号
    void set_next_page_id(uint32_t next_page) {
        next_page_id = next_page;
    }


    // 获取数据文件路径（调试用）
    string get_data_file_path() const { return data_file_path; }

   
};

#endif // PAGE_MANAGER_H#pragma once
#pragma once
