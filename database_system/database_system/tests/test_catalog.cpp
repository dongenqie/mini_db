#include "../engine/catalog_manager.hpp"
#include <iostream>

int main() {
    // Create CatalogManager with catalog.db file
    CatalogManager manager("catalog.db");
    Catalog catalog;

    // Try to load catalog from file (if exists)
    if (manager.LoadCatalog(catalog)) {
        std::cout << "Catalog loaded successfully!\n";
    }
    else {
        std::cout << "Catalog file does not exist, starting with an empty catalog.\n";
    }

    // Create schema for the "student" table
    std::vector<Column> studentCols = {
        Column("id", ColumnType::INT),
        Column("name", ColumnType::VARCHAR, 50),
        Column("gpa", ColumnType::FLOAT)
    };
    Schema studentSchema(studentCols);

    // Create a new "student" table
    if (manager.CreateTable(catalog, "student", studentSchema, "student.tbl")) {
        std::cout << "'student' table created successfully!\n";
    }
    else {
        std::cout << "'student' table already exists.\n";
    }

    // List all tables in the catalog
    std::cout << "\nCurrent tables in the catalog:\n";
    for (const auto& name : catalog.ListTables()) {
        std::cout << " - " << name << "\n";
    }

    // Drop the "student" table
    if (manager.DropTable(catalog, "student")) {
        std::cout << "\n'student' table dropped successfully.\n";
    }
    else {
        std::cout << "\n'student' table not found to drop.\n";
    }

    // List tables after deletion
    std::cout << "\nTables after deletion:\n";
    for (const auto& name : catalog.ListTables()) {
        std::cout << " - " << name << "\n";
    }

    return 0;
}
