// =============================================
// storage/page.cpp
// =============================================
#include "page.hpp"
#include <stdexcept>
#include <cstring>

using namespace std;

#pragma pack(push,1)
struct PageHeaderPack {
    uint32_t page_id;
    uint32_t free_offset;
    uint32_t prev_page_id;
    uint32_t next_page_id;
};
#pragma pack(pop)

// ���л�����ҳͷԪ��Ϣ��ҳ�š�����ƫ�ơ�����ҳ�ţ�д��data����ǰ16�ֽ�
void Page::serialize() {
    PageHeaderPack h{ page_id, free_offset, prev_page_id, next_page_id };
    std::memcpy(data, &h, sizeof(h));
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
        throw invalid_argument("page.cpp���������л�ʧ��: �ֽ���Ϊ��");
    }
    // �Ƚ��������ݿ�������ǰҳ��data����
    memcpy(data, disk_data, PAGE_SIZE);

    // �ٰ�ҳͷ�������Ա����
    PageHeaderPack h{};
    std::memcpy(&h, data, sizeof(h));
    page_id = h.page_id;
    free_offset = h.free_offset;
    prev_page_id = h.prev_page_id;
    next_page_id = h.next_page_id;
}