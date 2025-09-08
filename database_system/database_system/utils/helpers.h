// =============================================
// utils/helpers.hpp（目前只放占位，便于扩展日志/断言等）
// =============================================
#pragma once
#include <iostream>
namespace minidb { inline void log(const std::string& s) { std::cerr << s << "\n"; } }
