#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

// StorageEngine: quản lý lưu trữ record vào file .tbl
class StorageEngine {
public:
    // Ghi thêm một record vào file
    static bool Insert(const std::string& fileName, const std::vector<std::string>& values);

    // Đọc toàn bộ record từ file
    static std::vector<std::vector<std::string>> Select(const std::string& fileName);

    // Xóa record theo điều kiện cột = giá trị
    static bool Delete(const std::string& fileName, int whereColIndex, const std::string& whereVal);

    // Xóa file (DROP TABLE)
    static bool DropTable(const std::string& fileName);
};
