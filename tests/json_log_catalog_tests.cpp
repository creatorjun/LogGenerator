// tests/json_log_catalog_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "infrastructure/json_log_catalog.hpp"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace loggen::tests {

void run_json_log_catalog_tests() {
    const infrastructure::JsonLogCatalog catalog;
    const auto source = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "sample_logs.json";
    const auto items = catalog.load(source);
    expect(items.size() == 345, "Expected 345 migrated sample logs");
    expect(!items.front().id.empty(), "First sample log id is empty");
    expect(!items.front().name.empty(), "First sample log name is empty");
    expect(!items.front().sample.empty(), "First sample log body is empty");
    expect(items.front().id == "sample-0001", "Migrated JSON sample ids were not normalized");
    expect(items.front().source == "기본 내장 샘플", "Migrated JSON sample source was not normalized");

    std::size_t timestamp_templates = 0;
    std::size_t source_ip_templates = 0;
    std::size_t destination_ip_templates = 0;
    bool found_separated_date = false;
    bool found_separated_time = false;
    bool found_month_day_year = false;
    for (const auto& item : items) {
        const auto analysis = application::LogRenderer::analyze(item.sample);
        timestamp_templates += analysis.timestamp_count > 0 ? 1 : 0;
        source_ip_templates += analysis.source_ip_count > 0 ? 1 : 0;
        destination_ip_templates += analysis.destination_ip_count > 0 ? 1 : 0;
        for (const auto style : analysis.timestamp_styles) {
            found_separated_date = found_separated_date || style == application::TimestampStyle::DateOnly;
            found_separated_time = found_separated_time || style == application::TimestampStyle::TimeOnly;
            found_month_day_year = found_month_day_year || style == application::TimestampStyle::MonthDayYear;
        }
    }
    expect(timestamp_templates >= 250, "Too few migrated templates have recognized timestamps");
    expect(source_ip_templates >= 100, "Too few migrated templates have recognized source IP fields");
    expect(destination_ip_templates >= 90, "Too few migrated templates have recognized destination IP fields");
    expect(found_separated_date && found_separated_time, "Separated date and time formats were not found in migrated samples");
    expect(found_month_day_year, "Month-first date format was not found in migrated samples");

    const auto directory = std::filesystem::current_path() / (".test_json_catalog_" + std::to_string(GetCurrentProcessId()));
    const auto file = directory / "sample_logs.json";
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    const std::vector<domain::LogTemplate> expected{
        {"custom-1", "사용자 로그", "timestamp=2030-01-02T03:04:05Z src_ip=10.0.0.1 dst_ip=10.0.0.2", "사용자 정의"},
        {"custom-2", "Multiline", "line one\nline two", "사용자 정의"},
    };
    catalog.save(file, expected);
    const auto actual = catalog.load(file);
    expect(actual.size() == expected.size(), "JSON catalog round trip changed item count");
    expect(actual[0].id == expected[0].id, "JSON catalog round trip changed id");
    expect(actual[0].name == expected[0].name, "JSON catalog round trip changed UTF-8 name");
    expect(actual[0].sample == expected[0].sample, "JSON catalog round trip changed sample");
    expect(actual[1].sample == expected[1].sample, "JSON catalog round trip changed multiline sample");
    const std::vector<domain::LogTemplate> empty;
    catalog.save(file, empty);
    expect(catalog.load(file).empty(), "JSON catalog could not persist an empty catalog");
    std::filesystem::remove_all(directory, cleanup_error);
}

}
