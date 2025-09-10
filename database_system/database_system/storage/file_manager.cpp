//ʵ��FileManager������г�Ա�������ļ�������Ԫ���ݳ־û����ӿڷ�װ��
#include "file_manager.hpp"
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include<iostream>

#include <windows.h>
using namespace std;

// -------------------------- ˽�и�����������ʼ�����ݿ�Ŀ¼ --------------------------
void FileManager::init_db_directory() {
    // ��Ŀ¼�����ڣ�����Ŀ¼
    if (CreateDirectoryA(db_dir.c_str(), NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            throw runtime_error("Create test dir failed: " + db_dir +
                ", error code: " + to_string(error));
        }
    }
}

// -------------------------- ˽�и�������������Ԫ���ݣ���meta.dat���� --------------------------
void FileManager::load_metadata() {
    // Ԫ�����ļ������ڣ���ʼ��Ĭ��Ԫ���ݣ�next_page_id=1��free_page_listΪ�գ�
    ifstream meta_file(meta_file_path, ios::in | ios::binary);
    if (!meta_file) {
        page_manager = PageManager(data_file_path); // PageManagerĬ�ϳ�ʼ��next_page_id=1
        return;
    }

    // 1. ��ȡnext_page_id��4�ֽ��޷���������
    uint32_t next_page_id;
    meta_file.read(reinterpret_cast<char*>(&next_page_id), sizeof(next_page_id));
    if (meta_file.gcount() != sizeof(next_page_id)) {
        meta_file.close();
        throw runtime_error("FileManager load metadata failed: read next_page_id error");
    }

    // 2. ��ȡ����ҳ�б��ȣ�4�ֽ��޷���������
    uint32_t free_page_count;
    meta_file.read(reinterpret_cast<char*>(&free_page_count), sizeof(free_page_count));
    if (meta_file.gcount() != sizeof(free_page_count)) {
        meta_file.close();
        throw runtime_error("FileManager load metadata failed: read free_page_count error");
    }

    // 3. ��ȡ����ҳ�б�ÿ��ҳ��4�ֽڣ�
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

    // 4. ��ʼ��PageManager�����ü��ص�Ԫ���ݣ�
    page_manager = PageManager(data_file_path);
    page_manager.set_next_page_id(next_page_id);
    page_manager.set_free_page_list(free_page_list);
}

// -------------------------- ˽�и�������������Ԫ���ݣ�д��meta.dat�� --------------------------
void FileManager::save_metadata() {
    // �ԡ�����д��ģʽ��Ԫ�����ļ���ȷ��Ԫ�������£�
    ofstream meta_file(meta_file_path, ios::out | ios::binary | ios::trunc);
    if (!meta_file) {
        throw runtime_error("FileManager save metadata failed: open meta file failed - " + meta_file_path);
    }

    // 1. д��next_page_id
    uint32_t next_page_id = page_manager.get_next_page_id();
    meta_file.write(reinterpret_cast<char*>(&next_page_id), sizeof(next_page_id));

    // 2. д�����ҳ�б���
    list<uint32_t> free_page_list = page_manager.get_free_page_list();
    uint32_t free_page_count = free_page_list.size();
    meta_file.write(reinterpret_cast<char*>(&free_page_count), sizeof(free_page_count));

    // 3. д�����ҳ�б�
    for (uint32_t page_id : free_page_list) {
        meta_file.write(reinterpret_cast<char*>(&page_id), sizeof(page_id));
    }

    meta_file.close();
}

// -------------------------- ���캯������ʼ��������� --------------------------
FileManager::FileManager(const string& db_dir, uint32_t cache_cap, ReplacePolicy policy)
    : db_dir(db_dir),
    // ��ʼ���ļ�·����Ŀ¼+Ĭ���ļ�����
    data_file_path(db_dir + "/" + DATA_FILE_NAME),
    meta_file_path(db_dir + "/" + META_FILE_NAME),
    // ��ʼ��PageManager���ݲ�����Ԫ���ݣ�����load_metadata���£�
    page_manager(data_file_path),
    // ��ʼ��CacheManager������PageManager����־�ļ��������ݿ�Ŀ¼��
    cache_manager(page_manager, cache_cap, policy, db_dir + "/cache_log.txt") {
    // 1. ��ʼ�����ݿ�Ŀ¼
    init_db_directory();
    // 2. ����Ԫ���ݣ���meta.dat�ָ�ҳ����״̬��
    load_metadata();
    // 3. ���³�ʼ��CacheManager��ȷ��PageManager�Ѽ���Ԫ���ݣ�
    //cache_manager = CacheManager(page_manager, cache_cap, policy, db_dir + "/cache_log.txt");
}

// -------------------------- ����������ȷ�����ݳ־û� --------------------------
FileManager::~FileManager() {
    try {
        // 1. ˢ�»�����������ҳ�������ļ�
        cache_manager.flush_all();
        // 2. ����Ԫ���ݵ�meta.dat
        save_metadata();
    }
    catch (const exception& e) {
        // �����������׳��쳣������ӡ������־
        cerr << "FileManager destructor warning: " << e.what() << endl;
    }
}

// -------------------------- ָ����ͳһ�ӿڣ�����ҳ --------------------------
uint32_t FileManager::allocate_page() {
    // ����PageManager����ҳ������ҳ��
    uint32_t page_id = page_manager.allocate_page();
    // ����������Ԫ���ݣ���������ʱ�ᱣ�棬�˴���ʡ��ʵʱ����������Ч�ʣ�
    return page_id;
}

// -------------------------- ָ����ͳһ�ӿڣ��ͷ�ҳ --------------------------
bool FileManager::free_page(uint32_t page_id) {
    // 1. ��ˢ�¸�ҳ�Ļ��棨��Ϊ��ҳ���������ݶ�ʧ��
    cache_manager.flush_page(page_id);
    // 2. ����PageManager�ͷ�ҳ
    return page_manager.free_page(page_id);
}

// -------------------------- ָ����ͳһ�ӿڣ���ҳ --------------------------
Page* FileManager::read_page(uint32_t page_id) {
    // ���ȴӻ����������CacheManager��get_page��δ�������Զ��������ļ���
    return cache_manager.get_page(page_id);
}

// -------------------------- ָ����ͳһ�ӿڣ�дҳ --------------------------
bool FileManager::write_page(uint32_t page_id, const Page& page) {
    // 1. ���ҳ�źϷ��ԣ�ҳ������Page�����ҳ��һ�£�
    if (page.get_page_id() != page_id) {
        return false;
    }

    // 2. �ӻ����ȡҳ��������������뻺�棩
    Page* cache_page = cache_manager.get_page(page_id);
    if (cache_page == nullptr) {
        return false;
    }

    // 3. ����ҳ���ݵ����棨ͨ��write_data�ӿڣ�ȷ����������ȷд�룩
    // ����ջ���ҳ���ݣ�������������ݣ�
    char empty_data[PAGE_SIZE] = { 0 };
    cache_page->write_data(0, empty_data, PAGE_SIZE);
    // д����ҳ���ݣ���page��data�������������ݣ�
    cache_page->write_data(0, page.data, PAGE_SIZE);

    // 4. ��ǻ���ҳΪ��ҳ���ӳ�ˢ�̣�����I/OЧ�ʣ�
    // ע������CacheManager������mark_dirty�ӿڣ�˽�г�Աis_dirty�����ã�
    cache_manager.mark_dirty(page_id);

    return true;
}

// -------------------------- ָ����ͳһ�ӿڣ�ˢ��ָ��ҳ --------------------------
bool FileManager::flush_page(uint32_t page_id) {
    // ����CacheManager��flush_page����ҳд�������ļ������Ϊ����ҳ��
    return cache_manager.flush_page(page_id);
}

// -------------------------- ָ����ͳһ�ӿڣ�ˢ������ҳ --------------------------
void FileManager::flush_all_pages() {
    // ����CacheManager��flush_all��ˢ��������ҳ�������ļ���
    cache_manager.flush_all();
}

// -------------------------- �����ӿڣ���ȡ����ͳ����Ϣ --------------------------
void FileManager::get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const {
    cache_manager.get_cache_stats(hit, miss, hit_rate);
}

void FileManager::print_cache_stats() const {
    cache_manager.print_stats();
}