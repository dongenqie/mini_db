// =============================================
// engine/storage_engine.cpp  （新增/覆盖）
// =============================================
#include "storage_engine.hpp"
#include <sstream>
#include <iostream>
#include <cstring>
#include <cctype>

static constexpr uint32_t HEADER = PAGE_HEADER_SIZE;    // 16
static constexpr uint32_t PAGESZ = PAGE_SIZE;

static inline std::string trim_copy(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

bool StorageEngine::ensure_table_ready(const std::string & tableName) {
    TableInfo * t = catalog_.GetTable(tableName);
    if (!t) return false;
    // 目录未初始化：分配首页
    if (t->first_pid == 0) return InitTablePages(tableName);
    // 目录有页，但物理上读不到 -> 兜底重建一个单页链
    if (fm_.read_page(t->last_pid) == nullptr) {
        uint32_t pid = fm_.allocate_page();
        Page * p = fm_.read_page(pid);
        if (!p) return false;
        p->set_page_id(pid);
        p->set_prev_page_id(INVALID_PAGE_ID);
        p->set_next_page_id(INVALID_PAGE_ID);
        p->set_free_offset(HEADER);
        fm_.write_page(pid, *p);
        fm_.flush_page(pid); // 关键：落盘
        return cmgr_.UpdateTablePages(catalog_, tableName, pid, pid);
    }
    return true;
}


// 拼/拆 CSV（简单：按逗号分割；值中不含换行）
std::string StorageEngine::join_csv(const std::vector<std::string>& v) {
    std::ostringstream os;
    for (size_t i = 0; i < v.size(); ++i) { if (i) os << ","; os << v[i]; }
    return os.str();
}
void StorageEngine::split_csv_line(const std::string& line, std::vector<std::string>& out) {
    out.clear();
    std::stringstream ss(line);
    std::string val;
    while (std::getline(ss, val, ',')) {
        // trim
        auto ltrim = [](std::string& s) {
            size_t i = 0;
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
            if (i) s.erase(0, i);
            };
        auto rtrim = [](std::string& s) {
            while (!s.empty()) {
                char c = s.back();
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ';') s.pop_back();
                else break;
            }
            };
        ltrim(val);
        rtrim(val);
        // 去首尾引号（容忍 INSERT 时写了 'Alice' 或 "Alice"）
        if (val.size() >= 2) {
            if ((val.front() == '\'' && val.back() == '\'') ||
                (val.front() == '"' && val.back() == '"')) {
                val = val.substr(1, val.size() - 2);
            }
        }
        out.push_back(std::move(val));
    }
}


template<typename Fn>
bool StorageEngine::for_each_page(const TableInfo* t, Fn fn) {
    if (!t || t->first_pid == 0) return true;
    uint32_t pid = t->first_pid;
    while (pid != INVALID_PAGE_ID && pid != 0) {
        Page* p = fm_.read_page(pid);
        if (!p) return false;
        if (!fn(pid, *p)) return false;
        pid = p->get_next_page_id();
    }
    return true;
}

bool StorageEngine::InitTablePages(const std::string& tableName) {
    TableInfo* t = catalog_.GetTable(tableName);
    if (!t) return false;
    if (t->first_pid != 0) return true; // 已初始化

    // 分配首个数据页
    uint32_t pid = fm_.allocate_page();
    Page* p = fm_.read_page(pid);
    if (!p) return false;
    p->set_page_id(pid);
    p->set_prev_page_id(INVALID_PAGE_ID);
    p->set_next_page_id(INVALID_PAGE_ID);
    p->set_free_offset(HEADER); // 数据从 16 字节后开始
    fm_.write_page(pid, *p);
    fm_.flush_page(pid);

    // 目录更新 + 立刻持久化
    if (!cmgr_.UpdateTablePages(catalog_, tableName, pid, pid)) return false;
    (void)cmgr_.PersistCatalog(catalog_);   // <=== 关键新增
    // 回填到 catalog
    return cmgr_.UpdateTablePages(catalog_, tableName, pid, pid);
}

bool StorageEngine::append_to_page(uint32_t page_id, const std::string& line,
    uint32_t& new_free_off, bool& ok_written) {
    ok_written = false;
    Page* p = fm_.read_page(page_id);
    if (!p) return false;

    uint32_t free_off = p->get_free_offset();
    uint32_t need = 2 + (uint32_t)line.size(); // uint16 len + payload
    if (free_off + need > PAGESZ) { // 空间不足
        new_free_off = free_off;
        return true;
    }
    // 写入记录
    uint16_t len = (uint16_t)line.size();
    if (!p->write_data(free_off, (const char*)&len, 2)) return false;
    if (len && !p->write_data(free_off + 2, line.data(), len)) return false;
    p->set_free_offset(free_off + 2 + len);

    // 关键：把最新的元信息写回到 data[0..15]
    p->serialize();

    fm_.write_page(page_id, *p);

    ok_written = true;
    new_free_off = p->get_free_offset();
    return true;
}

bool StorageEngine::allocate_linked_page(uint32_t prev_pid, uint32_t& new_pid) {
    new_pid = fm_.allocate_page();
    Page* np = fm_.read_page(new_pid);
    if (!np) return false;
    np->set_page_id(new_pid);
    np->set_prev_page_id(prev_pid);
    np->set_next_page_id(INVALID_PAGE_ID);
    np->set_free_offset(HEADER);
    fm_.write_page(new_pid, *np);

    // 链接前页
    if (prev_pid != INVALID_PAGE_ID && prev_pid != 0) {
        Page* pp = fm_.read_page(prev_pid);
        if (!pp) return false;
        pp->set_next_page_id(new_pid);
        fm_.write_page(prev_pid, *pp);
    }
    fm_.flush_page(new_pid);
    if (prev_pid) fm_.flush_page(prev_pid);
    return true;
}

bool StorageEngine::Insert(const std::string& tableName, const std::vector<std::string>& values) {
    if (!ensure_table_ready(tableName)) return false;
    TableInfo * t = catalog_.GetTable(tableName);
    if (!t) return false;

    std::string payload = join_csv(values);

    // 先尝试当前最后一页
    uint32_t new_free = 0; bool written = false;
    if (!append_to_page(t->last_pid, payload, new_free, written)) return false;
    if (!written) {
        uint32_t npid = 0;
        if (!allocate_linked_page(t->last_pid, npid)) return false;

        if (!append_to_page(npid, payload, new_free, written) || !written) return false;

        // 更新 catalog 的 last_pid，并立即持久化
        if (!cmgr_.UpdateTablePages(catalog_, tableName, t->first_pid, npid)) return false;
        (void)cmgr_.PersistCatalog(catalog_);   // <=== 关键新增
    }
    else {
        // 写在当前最后页也更新一下（可选，但稳妥），以免 free_offset 变化时需要额外信息
        (void)cmgr_.PersistCatalog(catalog_);   // <=== 保守起见每次插入都落盘目录
    }
    // 更新 catalog 的 last_pid 后……
    fm_.flush_page(t->last_pid); // 让交互式 SELECT 立即可见
    return true;
}

std::vector<std::vector<std::string>> StorageEngine::SelectAll(const std::string& tableName) {
    std::vector<std::vector<std::string>> rows;
    TableInfo* t = catalog_.GetTable(tableName);
    if (!t || t->first_pid == 0) return rows;

    for_each_page(t, [&](uint32_t pid, Page& pg)->bool {
        uint32_t off = HEADER;
        uint32_t end = pg.get_free_offset();

        // 健壮性：free_offset 越界时兜底（避免空页被扫成“很多空行”）
        if (end > PAGESZ) end = PAGESZ;
        if (end < HEADER) end = HEADER;

        while (off + 2 <= end) {
            uint16_t len = 0;
            if (!pg.read_data(off, (char*)&len, 2)) break;
            off += 2;

            // 关键防御：len==0 或者越界，直接停止扫描当前页
            if (len == 0) break;
            if (off + len > end) break;

            std::string line(len, '\0');
            if (!pg.read_data(off, line.data(), len)) break;
            off += len;

            std::vector<std::string> rec;
            split_csv_line(line, rec);
            rows.push_back(std::move(rec));
        }
        return true;
        });
    return rows;
}

bool StorageEngine::DeleteWhere(const std::string& tableName, int whereColIndex, const std::string& whereVal) {
    TableInfo* t = catalog_.GetTable(tableName);
    if (!t) return false;

    // 1) 全表扫描，得出需要保留的记录
    auto all = SelectAll(tableName);
    std::vector<std::vector<std::string>> kept;
    kept.reserve(all.size());
    for (auto& r : all) {
        if (whereColIndex < 0 || whereColIndex >= (int)r.size()) {
            // 无 WHERE：全删 => kept 不入
            continue;
        }
        if (r[whereColIndex] != whereVal) kept.push_back(std::move(r));
    }

    // 2) 释放旧页链（记住 next 再 free）
    if (t->first_pid != 0) {
        uint32_t pid = t->first_pid;
        while (pid != INVALID_PAGE_ID && pid != 0) {
            Page* p = fm_.read_page(pid);
            if (!p) break;
            uint32_t next = p->get_next_page_id();
            fm_.free_page(pid);
            pid = next;
        }
    }

    // 3) 关键：把目录页链置空，彻底与旧链断开
    if (!cmgr_.UpdateTablePages(catalog_, tableName, 0, 0)) return false;

    // 4) 新建一个空链（为后续插入或空表 select 做准备）
    if (!InitTablePages(tableName)) return false;

    // 5) 把保留记录写回
    for (auto& r : kept) {
        if (!Insert(tableName, r)) return false;
    }
    return true;
}


bool StorageEngine::DropTableData(const std::string& tableName) {
    TableInfo* t = catalog_.GetTable(tableName);
    if (!t) return false;

    // 释放所有页
    if (t->first_pid != 0) {
        uint32_t pid = t->first_pid;
        while (pid != INVALID_PAGE_ID && pid != 0) {
            Page* p = fm_.read_page(pid);
            if (!p) break;
            uint32_t next = p->get_next_page_id();
            fm_.free_page(pid);
            pid = next;
        }
    }
    // 清空 pid + 持久化
    bool ok = cmgr_.UpdateTablePages(catalog_, tableName, 0, 0);
    (void)cmgr_.PersistCatalog(catalog_);
    return ok;
}
