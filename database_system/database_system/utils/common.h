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

namespace minidb {

	static constexpr size_t PAGE_SIZE = 4096;

	enum class DataType { INT32, VARCHAR };

	struct Column { std::string name; DataType type; };
	struct TableDef { std::string name; std::vector<Column> columns; };
	using Value = std::variant<int32_t, std::string>;
	struct Row { std::vector<Value> values; };

	struct Status { bool ok{ true }; std::string message; static Status OK() { return { true, "" }; } static Status Error(std::string m) { return { false,std::move(m) }; } };

	inline std::string trim(const std::string& s) { size_t a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) return ""; size_t b = s.find_last_not_of(" \t\r\n"); return s.substr(a, b - a + 1); }

} // namespace minidb