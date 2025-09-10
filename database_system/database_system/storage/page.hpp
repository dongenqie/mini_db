#ifndef PAGE_H
#define PAGE_H

#include <cstdint>
#include <cstring>
#include <string>

// ���ĳ�������
#define PAGE_SIZE 4096          // ÿҳ�̶���С��4KB
#define PAGE_HEADER_SIZE 16     // ҳͷ��С��16�ֽڣ��洢4��uint32_tԪ��Ϣ��
#define INVALID_PAGE_ID 0       // ��Чҳ�ţ�ҳ�Ŵ�1��ʼ��

class Page {
    // ����PageManagerΪ��Ԫ�࣬�������������˽�г�Ա
    friend class PageManager;
    friend class FileManager;

private:
    uint32_t page_id;          // Ψһҳ��ţ���1��
    uint32_t free_offset;      // ���������пռ���ʼƫ�ƣ���PAGE_HEADER_SIZE��ʼ��
    uint32_t prev_page_id;     // ��һҳ��ţ��������ݱ��ҳ������֯��ʵ�����ݱ�ӳ�䣩
    uint32_t next_page_id;     // ��һҳ���
    char data[PAGE_SIZE];      // ҳ�������ݣ�ҳͷ+����������4KB��

public:
    // ���캯������ʼ��ҳ�ţ�Ĭ�Ͽ���ƫ��Ϊҳͷ����������ʼλ�ã�
    Page(uint32_t pid = INVALID_PAGE_ID)
        : page_id(pid), free_offset(PAGE_HEADER_SIZE),
        prev_page_id(INVALID_PAGE_ID), next_page_id(INVALID_PAGE_ID) {
        memset(data, 0, PAGE_SIZE); // ��ʼ��������Ϊ0�����������ݲ���
    }

    // -------------------------- ҳԪ��Ϣ���ʽӿڣ���PageManager���ã� --------------------------
    uint32_t get_page_id() const { return page_id; }
    void set_page_id(uint32_t pid) { page_id = pid; }

    uint32_t get_free_offset() const { return free_offset; }

    // ���¿���ƫ�ƣ�ȷ��������ҳ��С���������������
    bool set_free_offset(uint32_t offset) {
        if (offset < PAGE_HEADER_SIZE || offset > PAGE_SIZE) return false;
        free_offset = offset;
        return true;
    }

    uint32_t get_prev_page_id() const { return prev_page_id; }
    void set_prev_page_id(uint32_t pid) { prev_page_id = pid; }

    uint32_t get_next_page_id() const { return next_page_id; }
    void set_next_page_id(uint32_t pid) { next_page_id = pid; }

    // -------------------------- ҳ���ݶ�д�ӿڣ������ݿ�洢������ã� --------------------------
    // ��������д�����ݣ���ָ��ƫ�ƿ�ʼ����ȷ��������ҳ��С��
    bool write_data(uint32_t offset, const char* data_buf, uint32_t len) {
        if (offset + len > PAGE_SIZE || data_buf == nullptr) return false;
        memcpy(data + offset, data_buf, len);
        return true;
    }

    // ����������ȡ���ݣ���ָ��ƫ�ƿ�ʼ����ȷ��������ҳ��С��
    bool read_data(uint32_t offset, char* data_buf, uint32_t len) const {
        if (offset + len > PAGE_SIZE || data_buf == nullptr) return false;
        memcpy(data_buf, data + offset, len);
        return true;
    }

    // -------------------------- ���л�/�����л���ҳ������ļ��ĸ�ʽת���� --------------------------
    // ���л�����ҳͷԪ��Ϣд��data���飨ǰ16�ֽڣ�������д�����
    void serialize();

    // �����л����Ӵ��̶�ȡ���ֽ����н���ҳͷԪ��Ϣ����ֵ����ǰPage����
    void deserialize(const char* disk_data);
};

#endif // PAGE_H
