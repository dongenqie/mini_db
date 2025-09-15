// =============================================
// utils/common.h
// =============================================
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <list>
#include <cctype>
#include <optional>
#include <algorithm>

namespace minidb {

	//static constexpr size_t PAGE_SIZE = 4096;

    enum class DataType {
        INT32,
        TINYINT,
        FLOAT,
        CHAR,
        VARCHAR,
        DECIMAL,
        TIMESTAMP
    };

    struct ColumnDef {
        std::string name;
        DataType    type{ DataType::VARCHAR };
        int         length{ 0 };   // CHAR/VARCHAR 的长度或 INT/TINYINT 的显示宽度
        int         scale{ 0 };    // DECIMAL 的 scale；precision 用 length 存
    };

    struct TableDef {
        std::string name;
        std::vector<ColumnDef> columns;
    };
	using Value = std::variant<int32_t, std::string>;
	struct Row { std::vector<Value> values; };

    struct Status {
        bool ok{ true };
        std::string message;
        static Status OK() { return { true, "" }; }
        static Status Error(std::string m) { return { false, std::move(m) }; }
    };

    inline std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    // ===== 新增：大小写不敏感/限定名工具（仅新增，不改旧接口） =====
    // 小工具：转小写
    inline std::string to_lower(const std::string& s) {
        std::string o = s; for (auto& c : o) c = (char)std::tolower((unsigned char)c); return o;
    }
    inline std::string to_upper(std::string s) {
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }
    inline bool ci_equal(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
        }
        return true;
    }
    // 拆分限定名： "t.c" -> {"t","c"}；"col" -> {"col",""}
    inline std::pair<std::string, std::string> split_qual_name(const std::string& n) {
        auto p = n.find('.');
        if (p == std::string::npos) return { n, "" };
        return { n.substr(0,p), n.substr(p + 1) };
    }

    // 大小写不敏感找列（语义/执行都会用到）
    inline std::optional<size_t> find_col_ci(const TableDef& td, const std::string& n) {
        auto tolow = [](const std::string& s) {
            std::string o = s; for (auto& c : o) c = (char)std::tolower((unsigned char)c); return o;
            };
        std::string tgt = tolow(n);
        for (size_t i = 0; i < td.columns.size(); ++i) {
            if (tolow(td.columns[i].name) == tgt) return i;
        }
        return std::nullopt;
    }
} // namespace minidb