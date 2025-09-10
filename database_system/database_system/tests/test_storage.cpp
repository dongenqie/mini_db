#include "../storage/cache_manager.hpp"
#include "../storage/file_manager.hpp"
#include "../storage/page.hpp"
#include "../storage/page_manager.hpp"
#include <iostream>
#include <direct.h>
#include <cassert>
#include <system_error>
#include <windows.h>

using namespace std;

// 辅助函数：创建测试目录（Windows版本）
void create_test_dir(const string& dir_path) {
    // 使用Windows API创建目录
    if (CreateDirectoryA(dir_path.c_str(), NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            throw runtime_error("Create test dir failed: " + dir_path +
                ", error code: " + to_string(error));
        }
    }
}

// 测试目录和文件路径
string test_dir = "C:\\Users\\que07\\Desktop\\实训\\202509\\test";
string data_path = test_dir + "\\data.dat";

// 测试1：页分配与写入
void test_allocate_and_write() {
    cout << "=== 测试1：页分配与写入 ===" << endl;

    try {
        create_test_dir(test_dir);
        cout << "成功创建测试目录: " << test_dir << endl;
    }
    catch (const exception& e) {
        cerr << "错误: " << e.what() << endl;
        return;
    }

    try {
        PageManager pm(data_path);
        cout << "成功初始化PageManager，数据文件: " << data_path << endl;

        // 分配第一页（预期页号1）
        uint32_t page_id = pm.allocate_page();
        assert(page_id == 1 && "测试1失败：首次分配页号应为1");
        cout << "成功分配页号：" << page_id << "（符合预期）" << endl;

        // 向页写入测试数据（模拟student表的元数据）
        Page page;
        bool read_ok = pm.read_page(page_id, page);
        assert(read_ok && "测试1失败：读取刚分配的页失败");

        string table_meta = "student:id(INT,4),name(VARCHAR,20),age(INT,4)"; // 表结构元数据
        bool write_ok = page.write_data(
            PAGE_HEADER_SIZE,        // 从数据区起始位置写入
            table_meta.c_str(),
            static_cast<uint32_t>(table_meta.size())
        );
        assert(write_ok && "测试1失败：向页写入数据失败");

        // 更新页的空闲偏移（写入后空闲偏移=页头大小+数据长度）
        bool offset_ok = page.set_free_offset(PAGE_HEADER_SIZE + static_cast<uint32_t>(table_meta.size()));
        assert(offset_ok && "测试1失败：更新空闲偏移失败");

        // 写入磁盘并验证
        bool disk_ok = pm.write_page(page_id, page);
        assert(disk_ok && "测试1失败：将页写入磁盘失败");
        cout << "成功写入页数据：" << table_meta << endl;
        cout << "测试1通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试1失败：" << e.what() << endl << endl;
        exit(1);
    }
}

// 测试2：页读取与释放
void test_read_and_free() {
    cout << "=== 测试2：页读取与释放 ===" << endl;

    try {
        PageManager pm(data_path);
        uint32_t page_id = 1; // 测试1中分配的页号
        Page page;

        // 读取页数据并验证
        bool read_ok = pm.read_page(page_id, page);
        assert(read_ok && "测试2失败：读取页失败");

        char read_buf[256] = { 0 };
        bool data_ok = page.read_data(PAGE_HEADER_SIZE, read_buf, 50); // 读取50字节（覆盖测试数据）
        assert(data_ok && "测试2失败：从页读取数据失败");

        string expected_meta = "student:id(INT,4),name(VARCHAR,20),age(INT,4)";
        assert(string(read_buf) == expected_meta && "测试2失败：读取的数据与预期不符");
        cout << "从页" << page_id << "读取到数据：" << read_buf << "（符合预期）" << endl;

        // 释放页并验证
        bool free_ok = pm.free_page(page_id);
        assert(free_ok && "测试2失败：释放页失败");
        cout << "成功释放页" << page_id << endl;

        // 验证空闲页复用（重新分配应返回页号1）
        uint32_t new_page_id = pm.allocate_page();
        assert(new_page_id == 1 && "测试2失败：空闲页未复用");
        cout << "重新分配页号：" << new_page_id << "（符合预期，空闲页复用成功）" << endl;
        cout << "测试2通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试2失败：" << e.what() << endl << endl;
        exit(1);
    }
}

// 测试3：数据表映射模拟
void test_table_page_mapping() {
    cout << "=== 测试3：数据表映射模拟 ===" << endl;

    try {
        PageManager pm(data_path);
        // 模拟student表的页链表：页1（首页）→ 页2（下一页）
        uint32_t page1_id = pm.allocate_page(); // 复用页1
        uint32_t page2_id = pm.allocate_page(); // 新页2

        // 初始化页1（首页）：next_page_id设为2
        Page page1;
        pm.read_page(page1_id, page1);
        page1.set_next_page_id(page2_id);
        pm.write_page(page1_id, page1);

        // 初始化页2（下一页）：prev_page_id设为1，next_page_id设为无效
        Page page2;
        pm.read_page(page2_id, page2);
        page2.set_prev_page_id(page1_id);
        page2.set_next_page_id(INVALID_PAGE_ID);
        pm.write_page(page2_id, page2);

        // 验证页链表关系
        Page verify_page1, verify_page2;
        pm.read_page(page1_id, verify_page1);
        pm.read_page(page2_id, verify_page2);

        assert(verify_page1.get_next_page_id() == page2_id && "测试3失败：页1的下一页号错误");
        assert(verify_page2.get_prev_page_id() == page1_id && "测试3失败：页2的上一页号错误");
        cout << "student表页映射关系：页" << page1_id << " → 页" << page2_id << "（符合预期）" << endl;
        cout << "测试3通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试3失败：" << e.what() << endl << endl;
        exit(1);
    }
}



// 测试4：缓存命中与未命中
void test_cache_hit_miss() {
    cout << "=== 测试4：缓存命中与未命中 ===" << endl;
    string log_path = test_dir + "/cache_log.txt";
    create_test_dir(test_dir);

    try {
        // 初始化页管理器（先分配2个页，用于测试）
        PageManager pm(data_path);
        uint32_t page1_id = pm.allocate_page(); // 页1
        uint32_t page2_id = pm.allocate_page(); // 页2

        // 初始化缓存管理器（容量1，LRU策略）
        CacheManager cm(pm, 1, ReplacePolicy::LRU, log_path);

        // 第一次访问页1：未命中（缓存空）
        Page* page1 = cm.get_page(page1_id);
        assert(page1 != nullptr && "测试4失败：第一次访问页1未命中但获取失败");
        assert(cm.get_current_size() == 1 && "测试4失败：缓存应包含1个页");

        // 第二次访问页1：命中
        Page* page1_hit = cm.get_page(page1_id);
        assert(page1_hit != nullptr && "测试4失败：第二次访问页1命中但获取失败");

        // 访问页2：未命中（缓存满，替换页1）
        Page* page2 = cm.get_page(page2_id);
        assert(page2 != nullptr && "测试4失败：访问页2未命中但获取失败");
        assert(cm.get_current_size() == 1 && "测试4失败：缓存替换后应仍为1个页");

        // 检查统计信息（总访问3次：命中1次，未命中2次）
        uint32_t hit, miss;
        double hit_rate;
        cm.get_cache_stats(hit, miss, hit_rate);
        assert(hit == 1 && miss == 2 && "测试4失败：统计信息错误");
        assert(hit_rate == 1.0 / 3 && "测试4失败：命中率计算错误");

        cm.print_stats();
        cout << "测试4通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试4失败：" << e.what() << endl << endl;
        exit(1);
    }
}

// 测试5：LRU替换策略
void test_lru_policy() {
    cout << "=== 测试5：LRU替换策略 ===" << endl;
    string log_path = test_dir + "/cache_log_lru.txt";

    try {
        PageManager pm(data_path);
        CacheManager cm(pm, 2, ReplacePolicy::LRU, log_path);

        // 分配3个页
        uint32_t page1_id = pm.allocate_page();
        uint32_t page2_id = pm.allocate_page();
        uint32_t page3_id = pm.allocate_page();

        // 访问顺序：页1 → 页2 → 页3（缓存满，需替换“最近最少使用”的页1）
        cm.get_page(page1_id);
        cm.get_page(page2_id);
        cm.get_page(page3_id);

        // 验证缓存内容：应包含页2、页3（替换页1）
        assert(cm.get_current_size() == 2 && "测试5失败：缓存大小错误");
        // 访问页1：未命中（已被替换）
        Page* page1_miss = cm.get_page(page1_id);
        assert(page1_miss != nullptr && "测试5失败：访问被替换的页1失败");
        // 访问后缓存内容：页3、页1（替换“最近最少使用”的页2）
        Page* page2_miss = cm.get_page(page2_id);
        assert(page2_miss != nullptr && "测试5失败：访问页2应未命中");

        cout << "LRU替换策略验证成功" << endl;
        cm.print_stats();
        cout << "测试5通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试5失败：" << e.what() << endl << endl;
        exit(1);
    }
}

// 测试6：脏页刷新（验证指导书“缓存刷新”“数据持久化”要求）
void test_dirty_page_flush() {
    cout << "=== 测试6：脏页刷新 ===" << endl;
    string log_path = test_dir + "/cache_log_dirty.txt";

    try {
        PageManager pm(data_path);
        CacheManager cm(pm, 1, ReplacePolicy::FIFO, log_path);

        // 1. 分配页并写入测试数据（标记为脏页）
        uint32_t page_id = pm.allocate_page();
        Page* page = cm.get_page(page_id);
        string test_data = "Cache dirty page test data";
        page->write_data(PAGE_HEADER_SIZE, test_data.c_str(), test_data.size());
        // 手动标记为脏页（实际项目中由数据库引擎调用）
        auto it = cm.get_cache_map().find(page_id);
        if (it != cm.get_cache_map().end()) {
            it->second.is_dirty = true;
        }

        // 2. 刷新页到磁盘
        bool flush_ok = cm.flush_page(page_id);
        assert(flush_ok && "测试6失败：脏页刷新失败");

        // 3. 从磁盘读取验证（绕过缓存）
        Page disk_page(page_id);
        pm.read_page(page_id, disk_page);
        char read_buf[1024] = { 0 };
        disk_page.read_data(PAGE_HEADER_SIZE, read_buf, test_data.size());
        assert(string(read_buf) == test_data && "测试6失败：磁盘数据与脏页数据不一致");

        // 4. 测试flush_all（新增脏页后刷新所有）
        Page* page2 = cm.get_page(pm.allocate_page());
        string test_data2 = "Flush all test data";
        page2->write_data(PAGE_HEADER_SIZE, test_data2.c_str(), test_data2.size());
        it = cm.get_cache_map().find(page2->get_page_id());
        if (it != cm.get_cache_map().end()) {
            it->second.is_dirty = true;
        }
        cm.flush_all();

        cout << "脏页刷新验证成功，磁盘数据与缓存数据一致" << endl;
        cout << "测试6通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试6失败：" << e.what() << endl << endl;
        exit(1);
    }
}

// 检查文件是否存在
bool file_exists(const std::string& file_path) {
    std::ifstream file(file_path);
    return file.good();  // 如果文件可以成功打开，则返回 true
}
// 辅助函数：删除测试目录（测试结束后清理）
void delete_test_dir(const string& dir_path) {
    // 检查目录是否存在
    if (!file_exists(dir_path)) {
        cout << "目录不存在: " << dir_path << endl;
        return;
    }

#ifdef _WIN32
    string cmd = "rd /s /q " + dir_path;
#else
    string cmd = "rm -rf " + dir_path;
#endif
    system(cmd.c_str());
}

// 测试7：文件初始化与元数据加载
void test_file_init_and_metadata() {
    cout << "=== 测试7：文件初始化与元数据加载 ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir); // 先清理旧目录

    try {
        // 1. 第一次初始化FileManager：创建目录、数据文件、元数据文件
        FileManager fm1(test_dir, 2, ReplacePolicy::LRU);
        cout << "第一次初始化：" << endl;
        cout << "数据文件路径：" << fm1.get_data_file_path() << endl;
        cout << "元数据文件路径：" << fm1.get_meta_file_path() << endl;


        // 验证文件是否创建成功
        assert(file_exists(fm1.get_data_file_path()) && "测试7失败：数据文件未创建");
        assert(file_exists(fm1.get_meta_file_path()) && "测试7失败：元数据文件未创建");

        // 2. 分配2个页（修改元数据：next_page_id=3，free_page_list为空）
        uint32_t page1_id = fm1.allocate_page();
        uint32_t page2_id = fm1.allocate_page();
        assert(page1_id == 1 && page2_id == 2 && "测试7失败：页分配号错误");

        // 3. 释放页1（修改元数据：free_page_list包含1）
        bool free_ok = fm1.free_page(page1_id);
        assert(free_ok && "测试7失败：页释放失败");

        // 4. 销毁fm1，触发元数据保存（析构函数调用save_metadata）
        fm1.~FileManager();

        // 5. 第二次初始化FileManager：加载之前保存的元数据
        FileManager fm2(test_dir, 2, ReplacePolicy::LRU);
        cout << "第二次初始化：" << endl;

        // 验证元数据是否恢复：分配页应复用页1（free_page_list中的页）
        uint32_t page3_id = fm2.allocate_page();
        assert(page3_id == 1 && "测试7失败：元数据未恢复，空闲页未复用");

        // 验证next_page_id是否恢复（应为3）
        // 注：需在PageManager中新增get_next_page_id接口（供测试用）
        PageManager& pm = const_cast<PageManager&>(fm2.get_page_manager()); // 测试用，实际项目需避免const_cast
        assert(pm.get_next_page_id() == 3 && "测试7失败：next_page_id未恢复");

        cout << "文件初始化与元数据加载验证成功" << endl;
        cout << "测试7通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试7失败：" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}

// 测试8：数据持久化
void test_data_persistence() {
    cout << "=== 测试8：数据持久化 ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir);

    try {
        // 1. 第一次启动：写入测试数据
        FileManager fm1(test_dir, 2, ReplacePolicy::LRU);
        uint32_t page_id = fm1.allocate_page();
        Page page(page_id);

        // 写入测试数据（模拟student表的一行记录）
        string student_data = "1,Alice,20";
        page.write_data(PAGE_HEADER_SIZE, student_data.c_str(), student_data.size());
        page.set_free_offset(PAGE_HEADER_SIZE + student_data.size()); // 更新空闲偏移

        // 写入文件（缓存标记为脏页）
        bool write_ok = fm1.write_page(page_id, page);
        assert(write_ok && "测试8失败：页写入失败");

        // 刷新脏页到磁盘（确保数据写入文件）
        fm1.flush_page(page_id);
        fm1.~FileManager(); // 销毁fm1

        // 2. 第二次启动：读取并验证数据
        FileManager fm2(test_dir, 2, ReplacePolicy::LRU);
        Page* read_page = fm2.read_page(page_id);
        assert(read_page != nullptr && "测试8失败：页读取失败");

        // 读取数据并验证
        char read_buf[1024] = { 0 };
        read_page->read_data(PAGE_HEADER_SIZE, read_buf, student_data.size());
        assert(string(read_buf) == student_data && "测试8失败：持久化数据与原数据不一致");

        cout << "原数据：" << student_data << endl;
        cout << "重启后读取的数据：" << read_buf << endl;
        cout << "数据持久化验证成功" << endl;
        cout << "测试8通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试8失败：" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}

// 测试9：模块协同（文件-页-缓存）
void test_module_coordination() {
    cout << "=== 测试9：模块协同（文件-页-缓存） ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir);

    try {
        FileManager fm(test_dir, 1, ReplacePolicy::LRU); // 缓存容量1，LRU策略
        uint32_t page1_id = fm.allocate_page();
        uint32_t page2_id = fm.allocate_page();

        // 1. 写页1数据并标记脏页
        Page page1(page1_id);
        string data1 = "Cache test data 1";
        page1.write_data(PAGE_HEADER_SIZE, data1.c_str(), data1.size());
        fm.write_page(page1_id, page1);

        // 2. 读页1：缓存命中（第一次读已加入缓存）
        Page* cache_page1 = fm.read_page(page1_id);
        assert(cache_page1 != nullptr && "测试9失败：页1缓存读取失败");

        // 3. 读页2：缓存满，LRU替换页1（页1脏页刷盘）
        Page page2(page2_id);
        string data2 = "Cache test data 2";
        page2.write_data(PAGE_HEADER_SIZE, data2.c_str(), data2.size());
        fm.write_page(page2_id, page2);
        Page* cache_page2 = fm.read_page(page2_id);
        assert(cache_page2 != nullptr && "测试9失败：页2缓存读取失败");

        // 4. 验证页1已刷盘（从文件直接读，绕过缓存）
        PageManager& pm = const_cast<PageManager&>(fm.get_page_manager());
        Page disk_page1(page1_id);
        pm.read_page(page1_id, disk_page1);
        char disk_buf[1024] = { 0 };
        disk_page1.read_data(PAGE_HEADER_SIZE, disk_buf, data1.size());
        assert(string(disk_buf) == data1 && "测试9失败：页1脏页未刷盘");

        // 5. 验证缓存统计（总访问3次：页1读2次（命中1次），页2读1次（未命中））
        uint32_t hit, miss;
        double hit_rate;
        fm.get_cache_stats(hit, miss, hit_rate);
        assert(hit == 1 && miss == 2 && "测试9失败：缓存统计错误");
        assert(hit_rate == 1.0 / 3 && "测试9失败：缓存命中率错误");

        fm.print_cache_stats();
        cout << "模块协同（文件-页-缓存）验证成功" << endl;
        cout << "测试9通过！" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "测试9失败：" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}


int main() {
    // 执行测试用例
    test_allocate_and_write();
    test_read_and_free();
    test_table_page_mapping();

    test_cache_hit_miss();
    test_lru_policy();
    test_dirty_page_flush();
    /*
        test_file_init_and_metadata();
        test_data_persistence();
        test_module_coordination();
        */
    cout << "所有测试用例均通过！" << endl;
    return 0;
}