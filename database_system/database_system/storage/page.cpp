#include "page.hpp"
#include <stdexcept>

using namespace std;

// ���л�����ҳͷԪ��Ϣ��ҳ�š�����ƫ�ơ�����ҳ�ţ�д��data����ǰ16�ֽ�
void Page::serialize() {
    // ��data�����׵�ַתΪuint32_tָ�룬��˳��д��Ԫ��Ϣ
    uint32_t* header_ptr = reinterpret_cast<uint32_t*>(data);
    *header_ptr++ = page_id;       // ��1-4�ֽڣ�ҳ��
    *header_ptr++ = free_offset;   // ��5-8�ֽڣ�����ƫ��
    *header_ptr++ = prev_page_id;  // ��9-12�ֽڣ���һҳ��
    *header_ptr = next_page_id;    // ��13-16�ֽڣ���һҳ��
}

// �����л����Ӵ��̶�ȡ���ֽ�����disk_data������ҳͷ�����µ�ǰPage����
void Page::deserialize(const char* disk_data) {
    if (disk_data == nullptr) {
        throw invalid_argument("Page deserialize failed: disk data is null");
    }
    // �Ƚ��������ݿ�������ǰҳ��data����
    memcpy(data, disk_data, PAGE_SIZE);
    // ��data�������ҳͷԪ��Ϣ
    uint32_t* header_ptr = reinterpret_cast<uint32_t*>(data);
    page_id = *header_ptr++;
    free_offset = *header_ptr++;
    prev_page_id = *header_ptr++;
    next_page_id = *header_ptr;
}