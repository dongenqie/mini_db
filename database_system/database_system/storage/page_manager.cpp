// =============================================
// storage/page_manager.cpp
// =============================================
#include "page_manager.hpp"
#include <algorithm>
#include <iostream>

using namespace std;
// ���캯������ʼ�������ļ��ͺ��Ĳ���
PageManager::PageManager(const string& data_path)
    : data_file_path(data_path), next_page_id(1) {
    init_data_file(); // ȷ�������ļ�����
}

// ҳ���䣺���ÿ���ҳ�򴴽���ҳ
uint32_t PageManager::allocate_page() {
    uint32_t page_id;
    if (!free_page_list.empty()) {
        page_id = free_page_list.front();
        free_page_list.pop_front();
    }
    else {
        page_id = next_page_id++;
        // Ԥ��չ�ļ�������ȷƫ��дһҳ��
        std::fstream fs(data_file_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) {
            // �ļ����ܻ�û����Ϊ�ɶ�д���� ofstream �ȴ����ٴ�
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


// ҳ�ͷţ���ҳ�ż�������б��߼��ͷţ�
bool PageManager::free_page(uint32_t page_id) {
    // �Ϸ��Լ�飺ҳ����Ч�����ڿ����б���
    if (page_id == INVALID_PAGE_ID) {
        return false;
    }
    auto it = find(free_page_list.begin(), free_page_list.end(), page_id);
    if (it != free_page_list.end()) {
        return false; // ҳ�ѿ��У������ظ��ͷ�
    }

    // ���ҳ���ݣ�����������Ϣ������������ݰ�ȫ�ԣ�
    Page empty_page(page_id);
    empty_page.serialize();
    if (!write_page(page_id, empty_page)) {
        return false;
    }

    // ��ҳ�ż�������б�
    free_page_list.push_back(page_id);
    return true;
}

// ҳ��ȡ���Ӵ��̶�ȡָ��ҳ�ŵ����ݵ�Page����
bool PageManager::read_page(uint32_t page_id, Page& page) {
    if (page_id == INVALID_PAGE_ID) {
        return false;
    }
    uint64_t offset = get_page_offset(page_id);

    std::ifstream data_file(data_file_path, std::ios::in | std::ios::binary);
    if (!data_file) {
        return false;
    }

    // ���ƫ���Ƿ�Խ��
    data_file.seekg(0, std::ios::end);
    std::streamoff file_size = data_file.tellg();
    if (file_size < 0 || offset + PAGE_SIZE > static_cast<uint64_t>(file_size)) {
        data_file.close();
        return false;
    }

    // ��ȡһҳ
    data_file.seekg(offset, std::ios::beg);
    char disk_page[PAGE_SIZE];
    data_file.read(disk_page, PAGE_SIZE);

    // �ڹر�ǰ����ȡ�ֽ���
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


// ҳд�룺��Page���������д����̶�Ӧҳλ��
bool PageManager::write_page(uint32_t page_id, const Page& page) {
    // �Ϸ��Լ�飺ҳ�Ų�ƥ�����Ч
    if (page_id == INVALID_PAGE_ID || page.get_page_id() != page_id) {
        return false;
    }
    uint64_t offset = get_page_offset(page_id);

    // �������ļ�����дģʽ������λ��ҳƫ��
    fstream data_file(data_file_path, ios::in | ios::out | ios::binary);
    if (!data_file) {
        return false;
    }
    data_file.seekp(offset, ios::beg);
    if (!data_file) { // ��λʧ�ܣ����ļ��𻵣�
        data_file.close();
        return false;
    }

    // ���л�Page����д�����
    Page temp_page = page;
    temp_page.serialize();
    data_file.write(temp_page.data, PAGE_SIZE);
    data_file.close();

    return true;
}