#include "../engine/executor.hpp"
#include "../engine/catalog_manager.hpp"
#include <iostream>

int main() {
    CatalogManager manager("catalog.db");
    Catalog catalog;

    // Load existing catalog
    manager.LoadCatalog(catalog);

    // Khởi tạo executor đúng cách
    Executor executor(manager, catalog);

    // Test CREATE TABLE
    if (executor.Execute("CREATE TABLE teacher (id INT, name VARCHAR(50)) FILE 'teacher.tbl';")) {
        std::cout << "Executor created table 'teacher' successfully.\n";
    }
    else {
        std::cout << "Failed to create table 'teacher'.\n";
    }

    // Test INSERT
    if (executor.Execute("INSERT INTO teacher VALUES (1, Alice);")) {
        std::cout << "Executor inserted into 'teacher'.\n";
    }

    // Test SELECT
    if (executor.Execute("SELECT id, name FROM teacher WHERE id = 1;")) {
        std::cout << "Executor selected from 'teacher'.\n";
    }

    // Test DELETE
    if (executor.Execute("DELETE FROM teacher WHERE id = 1;")) {
        std::cout << "Executor deleted from 'teacher'.\n";
    }

    // Test DROP TABLE
    if (executor.Execute("DROP TABLE teacher;")) {
        std::cout << "Executor dropped table 'teacher' successfully.\n";
    }

    // Test SHOW TABLES
    executor.Execute("SHOW TABLES;");

    return 0;
}
