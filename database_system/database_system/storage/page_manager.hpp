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
    string data_file_path;       // ���������ļ�·��
    uint32_t next_page_id;       // ��һ�����������ҳ�ţ���ʼΪ1��ȷ��ҳ��Ψһ��
    list<uint32_t> free_page_list; // ����ҳ�б�ά���ɸ��õ�ҳ�ţ����������Ƭ��

    // �ؼ�������ҳƫ�Ʊ����� (page_id - 1) * PAGE_SIZE
    uint64_t get_page_offset(uint32_t page_id) const {
        // ҳ�Ŵ� 1 ��ʼ
        return static_cast<uint64_t>(page_id - 1) * PAGE_SIZE;
    }

    // ������������ʼ�������ļ������������򴴽����ļ���ȷ��������д������
    void init_data_file() {
        ifstream file(data_file_path, ios::binary);
        if (!file) { // �ļ������ڣ����������ƿ��ļ�
            ofstream new_file(data_file_path, ios::binary);
            if (!new_file) {
                throw runtime_error("page_manager.hpp����ҳ�����ʼ��ʧ��: �����ļ� " + data_file_path + "ʧ��");
            }
            new_file.close();
        }
        else {
            file.close();
        }
    }

public:
 
    // ���캯������ʼ�������ļ�·��������ҳ�б���һҳ��
    PageManager(const string& data_path);

    // -------------------------- ���Ĺ��ܣ�ҳ���� --------------------------
    // ���ܣ��ӿ���ҳ�б���ҳ���޿���ҳ�������ҳ������ҳ�ţ�
    uint32_t allocate_page();

    // -------------------------- ���Ĺ��ܣ�ҳ�ͷ� --------------------------
    // ���ܣ���ҳ�ż�������б��߼��ͷţ��ݲ�����ɾ����
    bool free_page(uint32_t page_id);

    // -------------------------- ���Ľӿڣ�ҳ��ȡ��read_page�� --------------------------
    // ���ܣ�����ҳ�ŴӴ��̶�ȡҳ���ݣ������������page
    bool read_page(uint32_t page_id, Page& page);

    // -------------------------- ���Ľӿڣ�ҳд�루write_page�� --------------------------
    // ���ܣ���Page���������д����̶�Ӧҳλ��
    bool write_page(uint32_t page_id, const Page& page);

    // -------------------------- �����ӿڣ�������/���ݿ�ģ����ã� --------------------------
    // ��ȡ����ҳ�б�����ģ���ж�ҳ�Ƿ�ɸ��ã�
    list<uint32_t> get_free_page_list() const { return free_page_list; }
    // ���ÿ���ҳ�б�
    void set_free_page_list(const std::list<uint32_t>& free_list) {
        free_page_list = free_list;
    }


    // ��ȡ��һҳ�ţ�Ԫ���ݳ־û�ʱʹ�ã�
    uint32_t get_next_page_id() const { return next_page_id; }
    // ������һ��ҳ��
    void set_next_page_id(uint32_t next_page) {
        next_page_id = next_page;
    }


    // ��ȡ�����ļ�·���������ã�
    string get_data_file_path() const { return data_file_path; }

   
};

#endif // PAGE_MANAGER_H#pragma once
#pragma once
