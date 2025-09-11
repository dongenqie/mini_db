#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>

class StorageEngine {
public:
    // ==========================
    // INSERT
    // ==========================
    bool Insert(const std::string& fileName, const std::vector<std::string>& values) {
        std::ofstream ofs(fileName, std::ios::app);
        if (!ofs.is_open()) {
            std::cerr << "Error: cannot open file " << fileName << " for insert.\n";
            return false;
        }

        for (size_t i = 0; i < values.size(); i++) {
            ofs << values[i];
            if (i + 1 < values.size()) ofs << ",";
        }
        ofs << "\n";
        ofs.close();
        return true;
    }

    // ==========================
    // SELECT
    // ==========================
    std::vector<std::vector<std::string>> Select(const std::string& fileName) {
        std::vector<std::vector<std::string>> rows;
        std::ifstream ifs(fileName);
        if (!ifs.is_open()) {
            std::cerr << "Error: cannot open file " << fileName << " for select.\n";
            return rows;
        }

        std::string line;
        while (std::getline(ifs, line)) {
            std::vector<std::string> row;
            std::stringstream ss(line);
            std::string val;
            while (std::getline(ss, val, ',')) row.push_back(val);
            rows.push_back(row);
        }
        ifs.close();
        return rows;
    }

    // ==========================
    // DELETE
    // ==========================
    bool Delete(const std::string& fileName, int whereColIndex, const std::string& whereVal) {
        std::ifstream ifs(fileName);
        if (!ifs.is_open()) {
            std::cerr << "Error: cannot open file " << fileName << " for delete.\n";
            return false;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(ifs, line)) {
            std::stringstream ss(line);
            std::string val;
            std::vector<std::string> cols;
            while (std::getline(ss, val, ',')) cols.push_back(val);

            if (whereColIndex < 0 || whereColIndex >= (int)cols.size() || cols[whereColIndex] != whereVal) {
                lines.push_back(line); // giữ lại nếu không match điều kiện
            }
        }
        ifs.close();

        std::ofstream ofs(fileName, std::ios::trunc);
        for (auto& l : lines) ofs << l << "\n";
        ofs.close();
        return true;
    }

    // ==========================
    // DROP TABLE
    // ==========================
    bool DropTable(const std::string& fileName) {
        if (std::remove(fileName.c_str()) == 0) {
            return true;
        }
        std::cerr << "Error: cannot drop file " << fileName << "\n";
        return false;
    }
};
