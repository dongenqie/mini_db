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

// ������������������Ŀ¼��Windows�汾��
void create_test_dir(const string& dir_path) {
    // ʹ��Windows API����Ŀ¼
    if (CreateDirectoryA(dir_path.c_str(), NULL) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            throw runtime_error("Create test dir failed: " + dir_path +
                ", error code: " + to_string(error));
        }
    }
}

// ����Ŀ¼���ļ�·��
string test_dir = "C:\\Users\\que07\\Desktop\\ʵѵ\\202509\\test";
string data_path = test_dir + "\\data.dat";

// ����1��ҳ������д��
void test_allocate_and_write() {
    cout << "=== ����1��ҳ������д�� ===" << endl;

    try {
        create_test_dir(test_dir);
        cout << "�ɹ���������Ŀ¼: " << test_dir << endl;
    }
    catch (const exception& e) {
        cerr << "����: " << e.what() << endl;
        return;
    }

    try {
        PageManager pm(data_path);
        cout << "�ɹ���ʼ��PageManager�������ļ�: " << data_path << endl;

        // �����һҳ��Ԥ��ҳ��1��
        uint32_t page_id = pm.allocate_page();
        assert(page_id == 1 && "����1ʧ�ܣ��״η���ҳ��ӦΪ1");
        cout << "�ɹ�����ҳ�ţ�" << page_id << "������Ԥ�ڣ�" << endl;

        // ��ҳд��������ݣ�ģ��student���Ԫ���ݣ�
        Page page;
        bool read_ok = pm.read_page(page_id, page);
        assert(read_ok && "����1ʧ�ܣ���ȡ�շ����ҳʧ��");

        string table_meta = "student:id(INT,4),name(VARCHAR,20),age(INT,4)"; // ��ṹԪ����
        bool write_ok = page.write_data(
            PAGE_HEADER_SIZE,        // ����������ʼλ��д��
            table_meta.c_str(),
            static_cast<uint32_t>(table_meta.size())
        );
        assert(write_ok && "����1ʧ�ܣ���ҳд������ʧ��");

        // ����ҳ�Ŀ���ƫ�ƣ�д������ƫ��=ҳͷ��С+���ݳ��ȣ�
        bool offset_ok = page.set_free_offset(PAGE_HEADER_SIZE + static_cast<uint32_t>(table_meta.size()));
        assert(offset_ok && "����1ʧ�ܣ����¿���ƫ��ʧ��");

        // д����̲���֤
        bool disk_ok = pm.write_page(page_id, page);
        assert(disk_ok && "����1ʧ�ܣ���ҳд�����ʧ��");
        cout << "�ɹ�д��ҳ���ݣ�" << table_meta << endl;
        cout << "����1ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����1ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}

// ����2��ҳ��ȡ���ͷ�
void test_read_and_free() {
    cout << "=== ����2��ҳ��ȡ���ͷ� ===" << endl;

    try {
        PageManager pm(data_path);
        uint32_t page_id = 1; // ����1�з����ҳ��
        Page page;

        // ��ȡҳ���ݲ���֤
        bool read_ok = pm.read_page(page_id, page);
        assert(read_ok && "����2ʧ�ܣ���ȡҳʧ��");

        char read_buf[256] = { 0 };
        bool data_ok = page.read_data(PAGE_HEADER_SIZE, read_buf, 50); // ��ȡ50�ֽڣ����ǲ������ݣ�
        assert(data_ok && "����2ʧ�ܣ���ҳ��ȡ����ʧ��");

        string expected_meta = "student:id(INT,4),name(VARCHAR,20),age(INT,4)";
        assert(string(read_buf) == expected_meta && "����2ʧ�ܣ���ȡ��������Ԥ�ڲ���");
        cout << "��ҳ" << page_id << "��ȡ�����ݣ�" << read_buf << "������Ԥ�ڣ�" << endl;

        // �ͷ�ҳ����֤
        bool free_ok = pm.free_page(page_id);
        assert(free_ok && "����2ʧ�ܣ��ͷ�ҳʧ��");
        cout << "�ɹ��ͷ�ҳ" << page_id << endl;

        // ��֤����ҳ���ã����·���Ӧ����ҳ��1��
        uint32_t new_page_id = pm.allocate_page();
        assert(new_page_id == 1 && "����2ʧ�ܣ�����ҳδ����");
        cout << "���·���ҳ�ţ�" << new_page_id << "������Ԥ�ڣ�����ҳ���óɹ���" << endl;
        cout << "����2ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����2ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}

// ����3�����ݱ�ӳ��ģ��
void test_table_page_mapping() {
    cout << "=== ����3�����ݱ�ӳ��ģ�� ===" << endl;

    try {
        PageManager pm(data_path);
        // ģ��student���ҳ����ҳ1����ҳ���� ҳ2����һҳ��
        uint32_t page1_id = pm.allocate_page(); // ����ҳ1
        uint32_t page2_id = pm.allocate_page(); // ��ҳ2

        // ��ʼ��ҳ1����ҳ����next_page_id��Ϊ2
        Page page1;
        pm.read_page(page1_id, page1);
        page1.set_next_page_id(page2_id);
        pm.write_page(page1_id, page1);

        // ��ʼ��ҳ2����һҳ����prev_page_id��Ϊ1��next_page_id��Ϊ��Ч
        Page page2;
        pm.read_page(page2_id, page2);
        page2.set_prev_page_id(page1_id);
        page2.set_next_page_id(INVALID_PAGE_ID);
        pm.write_page(page2_id, page2);

        // ��֤ҳ�����ϵ
        Page verify_page1, verify_page2;
        pm.read_page(page1_id, verify_page1);
        pm.read_page(page2_id, verify_page2);

        assert(verify_page1.get_next_page_id() == page2_id && "����3ʧ�ܣ�ҳ1����һҳ�Ŵ���");
        assert(verify_page2.get_prev_page_id() == page1_id && "����3ʧ�ܣ�ҳ2����һҳ�Ŵ���");
        cout << "student��ҳӳ���ϵ��ҳ" << page1_id << " �� ҳ" << page2_id << "������Ԥ�ڣ�" << endl;
        cout << "����3ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����3ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}



// ����4������������δ����
void test_cache_hit_miss() {
    cout << "=== ����4������������δ���� ===" << endl;
    string log_path = test_dir + "/cache_log.txt";
    create_test_dir(test_dir);

    try {
        // ��ʼ��ҳ���������ȷ���2��ҳ�����ڲ��ԣ�
        PageManager pm(data_path);
        uint32_t page1_id = pm.allocate_page(); // ҳ1
        uint32_t page2_id = pm.allocate_page(); // ҳ2

        // ��ʼ�����������������1��LRU���ԣ�
        CacheManager cm(pm, 1, ReplacePolicy::LRU, log_path);

        // ��һ�η���ҳ1��δ���У�����գ�
        Page* page1 = cm.get_page(page1_id);
        assert(page1 != nullptr && "����4ʧ�ܣ���һ�η���ҳ1δ���е���ȡʧ��");
        assert(cm.get_current_size() == 1 && "����4ʧ�ܣ�����Ӧ����1��ҳ");

        // �ڶ��η���ҳ1������
        Page* page1_hit = cm.get_page(page1_id);
        assert(page1_hit != nullptr && "����4ʧ�ܣ��ڶ��η���ҳ1���е���ȡʧ��");

        // ����ҳ2��δ���У����������滻ҳ1��
        Page* page2 = cm.get_page(page2_id);
        assert(page2 != nullptr && "����4ʧ�ܣ�����ҳ2δ���е���ȡʧ��");
        assert(cm.get_current_size() == 1 && "����4ʧ�ܣ������滻��Ӧ��Ϊ1��ҳ");

        // ���ͳ����Ϣ���ܷ���3�Σ�����1�Σ�δ����2�Σ�
        uint32_t hit, miss;
        double hit_rate;
        cm.get_cache_stats(hit, miss, hit_rate);
        assert(hit == 1 && miss == 2 && "����4ʧ�ܣ�ͳ����Ϣ����");
        assert(hit_rate == 1.0 / 3 && "����4ʧ�ܣ������ʼ������");

        cm.print_stats();
        cout << "����4ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����4ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}

// ����5��LRU�滻����
void test_lru_policy() {
    cout << "=== ����5��LRU�滻���� ===" << endl;
    string log_path = test_dir + "/cache_log_lru.txt";

    try {
        PageManager pm(data_path);
        CacheManager cm(pm, 2, ReplacePolicy::LRU, log_path);

        // ����3��ҳ
        uint32_t page1_id = pm.allocate_page();
        uint32_t page2_id = pm.allocate_page();
        uint32_t page3_id = pm.allocate_page();

        // ����˳��ҳ1 �� ҳ2 �� ҳ3�������������滻���������ʹ�á���ҳ1��
        cm.get_page(page1_id);
        cm.get_page(page2_id);
        cm.get_page(page3_id);

        // ��֤�������ݣ�Ӧ����ҳ2��ҳ3���滻ҳ1��
        assert(cm.get_current_size() == 2 && "����5ʧ�ܣ������С����");
        // ����ҳ1��δ���У��ѱ��滻��
        Page* page1_miss = cm.get_page(page1_id);
        assert(page1_miss != nullptr && "����5ʧ�ܣ����ʱ��滻��ҳ1ʧ��");
        // ���ʺ󻺴����ݣ�ҳ3��ҳ1���滻���������ʹ�á���ҳ2��
        Page* page2_miss = cm.get_page(page2_id);
        assert(page2_miss != nullptr && "����5ʧ�ܣ�����ҳ2Ӧδ����");

        cout << "LRU�滻������֤�ɹ�" << endl;
        cm.print_stats();
        cout << "����5ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����5ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}

// ����6����ҳˢ�£���ָ֤���顰����ˢ�¡������ݳ־û���Ҫ��
void test_dirty_page_flush() {
    cout << "=== ����6����ҳˢ�� ===" << endl;
    string log_path = test_dir + "/cache_log_dirty.txt";

    try {
        PageManager pm(data_path);
        CacheManager cm(pm, 1, ReplacePolicy::FIFO, log_path);

        // 1. ����ҳ��д��������ݣ����Ϊ��ҳ��
        uint32_t page_id = pm.allocate_page();
        Page* page = cm.get_page(page_id);
        string test_data = "Cache dirty page test data";
        page->write_data(PAGE_HEADER_SIZE, test_data.c_str(), test_data.size());
        // �ֶ����Ϊ��ҳ��ʵ����Ŀ�������ݿ�������ã�
        auto it = cm.get_cache_map().find(page_id);
        if (it != cm.get_cache_map().end()) {
            it->second.is_dirty = true;
        }

        // 2. ˢ��ҳ������
        bool flush_ok = cm.flush_page(page_id);
        assert(flush_ok && "����6ʧ�ܣ���ҳˢ��ʧ��");

        // 3. �Ӵ��̶�ȡ��֤���ƹ����棩
        Page disk_page(page_id);
        pm.read_page(page_id, disk_page);
        char read_buf[1024] = { 0 };
        disk_page.read_data(PAGE_HEADER_SIZE, read_buf, test_data.size());
        assert(string(read_buf) == test_data && "����6ʧ�ܣ�������������ҳ���ݲ�һ��");

        // 4. ����flush_all��������ҳ��ˢ�����У�
        Page* page2 = cm.get_page(pm.allocate_page());
        string test_data2 = "Flush all test data";
        page2->write_data(PAGE_HEADER_SIZE, test_data2.c_str(), test_data2.size());
        it = cm.get_cache_map().find(page2->get_page_id());
        if (it != cm.get_cache_map().end()) {
            it->second.is_dirty = true;
        }
        cm.flush_all();

        cout << "��ҳˢ����֤�ɹ������������뻺������һ��" << endl;
        cout << "����6ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����6ʧ�ܣ�" << e.what() << endl << endl;
        exit(1);
    }
}

// ����ļ��Ƿ����
bool file_exists(const std::string& file_path) {
    std::ifstream file(file_path);
    return file.good();  // ����ļ����Գɹ��򿪣��򷵻� true
}
// ����������ɾ������Ŀ¼�����Խ���������
void delete_test_dir(const string& dir_path) {
    // ���Ŀ¼�Ƿ����
    if (!file_exists(dir_path)) {
        cout << "Ŀ¼������: " << dir_path << endl;
        return;
    }

#ifdef _WIN32
    string cmd = "rd /s /q " + dir_path;
#else
    string cmd = "rm -rf " + dir_path;
#endif
    system(cmd.c_str());
}

// ����7���ļ���ʼ����Ԫ���ݼ���
void test_file_init_and_metadata() {
    cout << "=== ����7���ļ���ʼ����Ԫ���ݼ��� ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir); // �������Ŀ¼

    try {
        // 1. ��һ�γ�ʼ��FileManager������Ŀ¼�������ļ���Ԫ�����ļ�
        FileManager fm1(test_dir, 2, ReplacePolicy::LRU);
        cout << "��һ�γ�ʼ����" << endl;
        cout << "�����ļ�·����" << fm1.get_data_file_path() << endl;
        cout << "Ԫ�����ļ�·����" << fm1.get_meta_file_path() << endl;


        // ��֤�ļ��Ƿ񴴽��ɹ�
        assert(file_exists(fm1.get_data_file_path()) && "����7ʧ�ܣ������ļ�δ����");
        assert(file_exists(fm1.get_meta_file_path()) && "����7ʧ�ܣ�Ԫ�����ļ�δ����");

        // 2. ����2��ҳ���޸�Ԫ���ݣ�next_page_id=3��free_page_listΪ�գ�
        uint32_t page1_id = fm1.allocate_page();
        uint32_t page2_id = fm1.allocate_page();
        assert(page1_id == 1 && page2_id == 2 && "����7ʧ�ܣ�ҳ����Ŵ���");

        // 3. �ͷ�ҳ1���޸�Ԫ���ݣ�free_page_list����1��
        bool free_ok = fm1.free_page(page1_id);
        assert(free_ok && "����7ʧ�ܣ�ҳ�ͷ�ʧ��");

        // 4. ����fm1������Ԫ���ݱ��棨������������save_metadata��
        fm1.~FileManager();

        // 5. �ڶ��γ�ʼ��FileManager������֮ǰ�����Ԫ����
        FileManager fm2(test_dir, 2, ReplacePolicy::LRU);
        cout << "�ڶ��γ�ʼ����" << endl;

        // ��֤Ԫ�����Ƿ�ָ�������ҳӦ����ҳ1��free_page_list�е�ҳ��
        uint32_t page3_id = fm2.allocate_page();
        assert(page3_id == 1 && "����7ʧ�ܣ�Ԫ����δ�ָ�������ҳδ����");

        // ��֤next_page_id�Ƿ�ָ���ӦΪ3��
        // ע������PageManager������get_next_page_id�ӿڣ��������ã�
        PageManager& pm = const_cast<PageManager&>(fm2.get_page_manager()); // �����ã�ʵ����Ŀ�����const_cast
        assert(pm.get_next_page_id() == 3 && "����7ʧ�ܣ�next_page_idδ�ָ�");

        cout << "�ļ���ʼ����Ԫ���ݼ�����֤�ɹ�" << endl;
        cout << "����7ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����7ʧ�ܣ�" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}

// ����8�����ݳ־û�
void test_data_persistence() {
    cout << "=== ����8�����ݳ־û� ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir);

    try {
        // 1. ��һ��������д���������
        FileManager fm1(test_dir, 2, ReplacePolicy::LRU);
        uint32_t page_id = fm1.allocate_page();
        Page page(page_id);

        // д��������ݣ�ģ��student���һ�м�¼��
        string student_data = "1,Alice,20";
        page.write_data(PAGE_HEADER_SIZE, student_data.c_str(), student_data.size());
        page.set_free_offset(PAGE_HEADER_SIZE + student_data.size()); // ���¿���ƫ��

        // д���ļ���������Ϊ��ҳ��
        bool write_ok = fm1.write_page(page_id, page);
        assert(write_ok && "����8ʧ�ܣ�ҳд��ʧ��");

        // ˢ����ҳ�����̣�ȷ������д���ļ���
        fm1.flush_page(page_id);
        fm1.~FileManager(); // ����fm1

        // 2. �ڶ�����������ȡ����֤����
        FileManager fm2(test_dir, 2, ReplacePolicy::LRU);
        Page* read_page = fm2.read_page(page_id);
        assert(read_page != nullptr && "����8ʧ�ܣ�ҳ��ȡʧ��");

        // ��ȡ���ݲ���֤
        char read_buf[1024] = { 0 };
        read_page->read_data(PAGE_HEADER_SIZE, read_buf, student_data.size());
        assert(string(read_buf) == student_data && "����8ʧ�ܣ��־û�������ԭ���ݲ�һ��");

        cout << "ԭ���ݣ�" << student_data << endl;
        cout << "�������ȡ�����ݣ�" << read_buf << endl;
        cout << "���ݳ־û���֤�ɹ�" << endl;
        cout << "����8ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����8ʧ�ܣ�" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}

// ����9��ģ��Эͬ���ļ�-ҳ-���棩
void test_module_coordination() {
    cout << "=== ����9��ģ��Эͬ���ļ�-ҳ-���棩 ===" << endl;
    string test_dir = test_dir + "/test_db_files";
    delete_test_dir(test_dir);

    try {
        FileManager fm(test_dir, 1, ReplacePolicy::LRU); // ��������1��LRU����
        uint32_t page1_id = fm.allocate_page();
        uint32_t page2_id = fm.allocate_page();

        // 1. дҳ1���ݲ������ҳ
        Page page1(page1_id);
        string data1 = "Cache test data 1";
        page1.write_data(PAGE_HEADER_SIZE, data1.c_str(), data1.size());
        fm.write_page(page1_id, page1);

        // 2. ��ҳ1���������У���һ�ζ��Ѽ��뻺�棩
        Page* cache_page1 = fm.read_page(page1_id);
        assert(cache_page1 != nullptr && "����9ʧ�ܣ�ҳ1�����ȡʧ��");

        // 3. ��ҳ2����������LRU�滻ҳ1��ҳ1��ҳˢ�̣�
        Page page2(page2_id);
        string data2 = "Cache test data 2";
        page2.write_data(PAGE_HEADER_SIZE, data2.c_str(), data2.size());
        fm.write_page(page2_id, page2);
        Page* cache_page2 = fm.read_page(page2_id);
        assert(cache_page2 != nullptr && "����9ʧ�ܣ�ҳ2�����ȡʧ��");

        // 4. ��֤ҳ1��ˢ�̣����ļ�ֱ�Ӷ����ƹ����棩
        PageManager& pm = const_cast<PageManager&>(fm.get_page_manager());
        Page disk_page1(page1_id);
        pm.read_page(page1_id, disk_page1);
        char disk_buf[1024] = { 0 };
        disk_page1.read_data(PAGE_HEADER_SIZE, disk_buf, data1.size());
        assert(string(disk_buf) == data1 && "����9ʧ�ܣ�ҳ1��ҳδˢ��");

        // 5. ��֤����ͳ�ƣ��ܷ���3�Σ�ҳ1��2�Σ�����1�Σ���ҳ2��1�Σ�δ���У���
        uint32_t hit, miss;
        double hit_rate;
        fm.get_cache_stats(hit, miss, hit_rate);
        assert(hit == 1 && miss == 2 && "����9ʧ�ܣ�����ͳ�ƴ���");
        assert(hit_rate == 1.0 / 3 && "����9ʧ�ܣ����������ʴ���");

        fm.print_cache_stats();
        cout << "ģ��Эͬ���ļ�-ҳ-���棩��֤�ɹ�" << endl;
        cout << "����9ͨ����" << endl << endl;
    }
    catch (const exception& e) {
        cerr << "����9ʧ�ܣ�" << e.what() << endl << endl;
        delete_test_dir(test_dir);
        exit(1);
    }

    delete_test_dir(test_dir);
}


int main() {
    // ִ�в�������
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
    cout << "���в���������ͨ����" << endl;
    return 0;
}