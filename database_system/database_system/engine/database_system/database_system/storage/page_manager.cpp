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
        // ���ÿ���ҳ��ȡ�б�ͷ��ҳ�ţ�FIFO���ò��ԣ�
        page_id = free_page_list.front();
        free_page_list.pop_front();
    }
    else {
        // �޿���ҳ��������ҳ�ţ�����ȷ��Ψһ��
        page_id = next_page_id++;
        // ��չ�����ļ�������һҳ��С�Ŀտռ䣨�������дҳʱƫ�Ƴ����ļ���С��
        ofstream data_file(data_file_path, ios::app | ios::binary);
        if (!data_file) {
            throw runtime_error("Page allocate failed: extend data file failed");
        }
        char empty_page[PAGE_SIZE] = { 0 }; // ��ҳ��ʼ����ȫ0��
        data_file.write(empty_page, PAGE_SIZE);
        data_file.close();
    }

    // ��ʼ����ҳ������ҳ�Ų�д����̣�
    Page new_page(page_id);
    new_page.serialize();
    if (!write_page(page_id, new_page)) {
        throw runtime_error("Page allocate failed: write new page to disk failed");
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
    // �Ϸ��Լ�飺ҳ����Ч��ƫ�Ƴ����ļ���С
    if (page_id == INVALID_PAGE_ID) {
        return false;
    }
    uint64_t offset = get_page_offset(page_id);

    // �������ļ�����λ��ҳƫ��
    ifstream data_file(data_file_path, ios::in | ios::binary);
    if (!data_file) {
        return false;
    }
    // ���ƫ���Ƿ񳬳��ļ��ܴ�С�������ȡ��Ч���ݣ�
    data_file.seekg(0, ios::end);
    uint64_t file_size = data_file.tellg();
    if (offset >= file_size) {
        data_file.close();
        return false;
    }

    // ��ȡһҳ���ݲ������л�
    data_file.seekg(offset, ios::beg);
    char disk_page[PAGE_SIZE];
    data_file.read(disk_page, PAGE_SIZE);
    data_file.close();

    // ����Ƿ�ɹ���ȡһ��ҳ
    if (data_file.gcount() != PAGE_SIZE) {
        return false;
    }

    try {
        page.deserialize(disk_page);
    }
    catch (const exception&) {
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