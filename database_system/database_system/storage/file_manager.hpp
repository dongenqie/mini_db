// =============================================
// storage/file_manager.hpp
// =============================================
//����FileManager�ࣨ�ļ���ʼ����Ԫ���ݶ�д��ͳһ�洢�ӿڡ�ģ��Эͬ��
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "page_manager.hpp"
#include "cache_manager.hpp"
#include <string>

// ���ݿ��ļ�Ĭ������
#define DATA_FILE_NAME "data.dat"    // �����ļ����洢ҳ���ݣ�
#define META_FILE_NAME "meta.dat"    // Ԫ�����ļ����洢ҳ����Ԫ���ݣ�

class FileManager {
private:
    std::string db_dir;              // ���ݿ��ļ���Ŀ¼
    std::string data_file_path;      // �����ļ�����·����db_dir + DATA_FILE_NAME��
    std::string meta_file_path;      // Ԫ�����ļ�����·����db_dir + META_FILE_NAME��

    // �����ĵײ�ģ��
    PageManager page_manager;        // ҳ���������Խ������ļ���
    CacheManager cache_manager;      // ������������Խ�ҳ��������

    // -------------------------- Ԫ���ݶ�д��˽�У����ڲ�ά���� --------------------------
    // ��meta.dat��ȡԪ���ݣ�next_page_id��free_page_list������ʼ��PageManager
    void load_metadata();
    // ��PageManager��Ԫ���ݣ�next_page_id��free_page_list��д��meta.dat��ʵ�ֳ־û�
    void save_metadata();
    // ��ʼ�����ݿ�Ŀ¼�����������򴴽���
    void init_db_directory();

public:
    // ���캯������ʼ��Ŀ¼���ļ�·�����ײ�ģ�飬����Ԫ����
    // ����˵����db_dir-���ݿ�Ŀ¼��cache_cap-����������policy-�����滻����
    FileManager(const std::string& db_dir, uint32_t cache_cap, ReplacePolicy policy);

    // ��������������Ԫ���ݣ�ˢ�����л�����ҳ��ȷ�����ݳ־û���ָ�������Ҫ��
    ~FileManager();

    // -------------------------- ָ����Ҫ��ͳһ�洢�ӿڣ������ݿ�ģ����ã� --------------------------
    // 1. ����ҳ������PageManager����ҳ������ҳ��
    uint32_t allocate_page();
    // 2. �ͷ�ҳ������PageManager�ͷ�ҳ����ҳ�ż�������б�
    bool free_page(uint32_t page_id);
    // 3. ��ҳ�����ȴӻ������δ��������ļ�������ҳָ�루 nullptr��ʾʧ�ܣ�
    Page* read_page(uint32_t page_id);
    // 4. дҳ��д�뻺�沢�����ҳ���ӳ�ˢ�̣�����Ч�ʣ�
    bool write_page(uint32_t page_id, const Page& page);
    // 5. ˢ��ҳ����ָ��ҳ�Ļ�����ҳд���ļ������Ϊ����ҳ
    bool flush_page(uint32_t page_id);
    // 6. ˢ�����У�ˢ�»�����������ҳ���ļ��������˳�/�����ύʱ���ã�
    void flush_all_pages();

    // -------------------------- �����ӿڣ�������/�����ã� --------------------------
    // ��ȡ����ͳ����Ϣ�����д�����δ���д����������ʣ�
    void get_cache_stats(uint32_t& hit, uint32_t& miss, double& hit_rate) const;
    // ��ӡ����ͳ����Ϣ������̨
    void print_cache_stats() const;
    // ��ȡ�����ļ�·��
    std::string get_data_file_path() const { return data_file_path; }
    // ��ȡԪ�����ļ�·��
    std::string get_meta_file_path() const { return meta_file_path; }

    // ���Ը����ӿڣ���ȡPageManager���ã��������ã�
    const PageManager& get_page_manager() const { return page_manager; }
};



#endif // FILE_MANAGER_H
