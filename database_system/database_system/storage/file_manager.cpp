//实现FileManager类的所有成员函数（文件创建、元数据持久化、接口封装）
#include "file_manager.hpp"
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include<iostream>

#include <windows.h>
using namespace std;

// -------------------------- 私有辅助函数：初始化数据库目录 --------------------------
void FileManager::init_db_directory() {
    // 若目录不存在，创建目录
    if (CreateDirectoryA(db_dir.c_str(), NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            throw runtime_error("Create test dir failed: " + db_dir +
                ", error code: " + to_string(error));
        }
    }
}

// -------------------------- 私有辅助函数：加载元数据（从meta.dat读） --------------------------
void FileManager::load_metadata() {
    // 元数据文件不存在：初始化默认元数据（next_page_id=1，free_page_list为空）
    ifstream meta_file(meta_file_path, ios::in | ios::binary);
    if (!meta_file) {
        page_manager = PageManager(data_file_path); // PageManager默认初始化next_page_id=1
        return;
    }

    // 1. 读取next_page_id（4字节无符号整数）
    uint32_t next_page_id;
    meta_file.read(reinterpret_cast<char*>(&next_page_id), sizeof(next_page_id));
    if (meta_file.gcount() != sizeof(next_page_id)) {
        meta_file.close();
        throw runtime_error("FileManager load metadata failed: read next_page_id error");
    }

    // 2. 读取空闲页列表长度（4字节无符号整数）
    uint32_t free_page_count;
    meta_file.read(reinterpret_cast<char*>(&free_page_count), sizeof(free_page_count));
    if (meta_file.gcount() != sizeof(free_page_count)) {
        meta_file.close();
        throw runtime_error("FileManager load metadata failed: read free_page_count error");
    }

    // 3. 读取空闲页列表（每个页号4字节）
    list<uint32_t> free_page_list;
    for (uint32_t i = 0; i < free_page_count; ++i) {
        uint32_t page_id;
        meta_file.read(reinterpret_cast<char*>(&page_id), sizeof(page_id));
        if (meta_file.gcount() != sizeof(page_id)) {
            meta_file.close();
            throw runtime_error("FileManager load metadata failed: read free_page_id error");
        }
        free_page_list.push_back(page_id);
    }

    meta_file.close();

    // 4. 初始化PageManager（设置加载的元数据）
    page_manager = PageManager(data_file_path);
    page_manager.set_next_page_id(next_page_id);
    page_manager.set_free_page_list(free_page_list);
}

// -------------------------- 私有辅助函数：保存元数据（写入meta.dat） --------------------------
void FileManager::save_metadata() {
    // 以“覆盖写”模式打开元数据文件（确保元数据最新）
    ofstream meta_file(meta_file_path, ios::out | ios::binary | ios::trunc);
    if (!meta_file) {
        throw runtime_error("FileManager save metadata failed: open meta file failed - " + meta_file_path);
    }

    // 1. 写入next_page_id
    uint32_t next_page_id = page_manager.get_next_page_id();
    meta_file.write(reinterpret_cast<char*>(&next_page_id), sizeof(next_page_id));

    // 2. 写入空闲页列表长度
    list<uint32_t> free_page_list = page_manager.get_free_page_list();
    uint32_t free_page_count = free_page_list.size();
    meta_file.write(reinterpret_cast<char*>(&free_page_count), sizeof(free_page_count));

    // 3. 写入空闲页列表
    for (uint32_t page_id : free_page_list) {
        meta_file.write(reinterpret_cast<char*>(&page_id), sizeof(page_id));
    }

    meta_file.close();
}

// -------------------------- 构造函数：初始化所有组件 --------------------------
FileManager::FileManager(const string& db_dir, uint32_t cache_cap, ReplacePolicy policy)
    : db_dir(db_dir),
    // 初始化文件路径（目录+默认文件名）
    data_file_path(db_dir + "/" + DATA_FILE_NAME),
    meta_file_path(db_dir + "/" + META_FILE_NAME),
    // 初始化PageManager（暂不设置元数据，后续load_metadata更新）
    page_manager(data_file_path),
    // 初始化CacheManager（关联PageManager，日志文件存入数据库目录）
    cache_manager(page_manager, cache_cap, policy, db_dir + "/cache_log.txt") {
    // 1. 初始化数据库目录
    init_db_directory();
    // 2. 加载元数据（从meta.dat恢复页管理状态）
    load_metadata();
    // 3. 重新初始化CacheManager（确保PageManager已加载元数据）
    //cache_manager = CacheManager(page_manager, cache_cap, policy, db_dir + "/cache_log.txt");
}

// -------------------------- 析构函数：确保数据持久化 --------------------------
FileManager::~FileManager() {
    try {
        // 1. 刷新缓存中所有脏页到数据文件
        cache_manager.flush_all();
        // 2. 保存元数据到meta.dat
        save_metadata();
    }
    catch (const exception& e) {
        // 析构函数不抛出异常，仅打印错误日志
        cerr << "FileManager destructor warning: " << e.what() << endl;
    }
}

// -------------------------- 指导书统一接口：分配页 --------------------------
uint32_t FileManager::allocate_page() {
    // 调用PageManager分配页，返回页号
    uint32_t page_id = page_manager.allocate_page();
    // 分配后需更新元数据（后续析构时会保存，此处可省略实时保存以提升效率）
    return page_id;
}

// -------------------------- 指导书统一接口：释放页 --------------------------
bool FileManager::free_page(uint32_t page_id) {
    // 1. 先刷新该页的缓存（若为脏页，避免数据丢失）
    cache_manager.flush_page(page_id);
    // 2. 调用PageManager释放页
    return page_manager.free_page(page_id);
}

// -------------------------- 指导书统一接口：读页 --------------------------
Page* FileManager::read_page(uint32_t page_id) {
    // 优先从缓存读：调用CacheManager的get_page（未命中则自动读数据文件）
    return cache_manager.get_page(page_id);
}

// -------------------------- 指导书统一接口：写页 --------------------------
bool FileManager::write_page(uint32_t page_id, const Page& page) {
    // 1. 检查页号合法性（页号需与Page对象的页号一致）
    if (page.get_page_id() != page_id) {
        return false;
    }

    // 2. 从缓存获取页（若不存在则加入缓存）
    Page* cache_page = cache_manager.get_page(page_id);
    if (cache_page == nullptr) {
        return false;
    }

    // 3. 复制页数据到缓存（通过write_data接口，确保数据区正确写入）
    // 先清空缓存页数据（避免残留脏数据）
    char empty_data[PAGE_SIZE] = { 0 };
    cache_page->write_data(0, empty_data, PAGE_SIZE);
    // 写入新页数据（从page的data区复制完整内容）
    cache_page->write_data(0, page.data, PAGE_SIZE);

    // 4. 标记缓存页为脏页（延迟刷盘，提升I/O效率）
    // 注：需在CacheManager中新增mark_dirty接口（私有成员is_dirty的设置）
    cache_manager.mark_dirty(page_id);

    return true;
}

// -------------------------- 指导书统一接口：刷新指定页 --------------------------
bool FileManager::flush_page(uint32_t page_id) {
    // 调用CacheManager的flush_page（脏页写入数据文件，标记为非脏页）
    return cache_manager.flush_page(page_id);
}

// -------------------------- 指导书统一接口：刷新所有页 --------------------------
void FileManager::flush_all_pages() {
    // 调用CacheManager的flush_all（刷新所有脏页到数据文件）
    cache_manager.flush_all();
}

// -------------------------- 辅助接口：获取缓存统计信息 --------------------------
void FileManager::get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const {
    cache_manager.get_cache_stats(hit, miss, hit_rate);
}

void FileManager::print_cache_stats() const {
    cache_manager.print_stats();
}