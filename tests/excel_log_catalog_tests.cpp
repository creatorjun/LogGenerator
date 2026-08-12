// tests/excel_log_catalog_tests.cpp
#include "test_support.hpp"

#include "infrastructure/excel_log_catalog.hpp"

#include <filesystem>
#include <string>

namespace loggen::tests {

void run_excel_log_catalog_tests() {
    infrastructure::ExcelLogCatalog catalog;
    const auto directory = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs";
    const auto items = catalog.load(directory);
    expect(items.size() >= 300, "Expected at least 300 sample logs from the workbook");
    expect(!items.front().name.empty(), "First sample log name is empty");
    expect(!items.front().sample.empty(), "First sample log body is empty");
    expect(items.front().source.find("Sample_Log_20260626.xlsx") != std::string::npos, "Workbook source metadata is missing");
}

}
