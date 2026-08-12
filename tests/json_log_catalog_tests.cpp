// tests/json_log_catalog_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "infrastructure/json_log_catalog.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace loggen::tests {

void run_json_log_catalog_tests() {
    const auto source = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "sample_logs.json";
    const infrastructure::JsonLogCatalog catalog{source};
    const auto items = catalog.load();
    expect(items.size() == 84, "Expected 84 CSV replacement sample logs");
    expect(!items.front().id.empty(), "First sample log id is empty");
    expect(!items.front().name.empty(), "First sample log name is empty");
    expect(!items.front().sample.empty(), "First sample log body is empty");
    expect(items.front().id == "csv-0001", "CSV sample ids were not normalized");

    std::size_t timestamp_templates = 0;
    std::size_t source_ip_templates = 0;
    std::size_t destination_ip_templates = 0;
    std::size_t privacy_templates = 0;
    std::size_t person_tokens = 0;
    std::size_t store_tokens = 0;
    const std::regex ipv4_pattern{R"(\b(?:(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\b)", std::regex::ECMAScript};
    for (const auto& item : items) {
        expect(item.id.starts_with("csv-"), "An old sample catalog entry remains");
        std::string lowercase_text = item.name + item.sample;
        std::ranges::transform(lowercase_text, lowercase_text.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        expect(lowercase_text.find("lotte") == std::string::npos && lowercase_text.find("mart") == std::string::npos && lowercase_text.find("yourcompany") == std::string::npos, "A requested company token remains");
        expect(item.sample.find("당진점") == std::string::npos, "A real store name remains");
        std::smatch ipv4_match;
        const auto has_ipv4 = std::regex_search(item.sample, ipv4_match, ipv4_pattern);
        expect(!has_ipv4, "An original IPv4 address remains in CSV catalog item " + item.id + " length " + std::to_string(ipv4_match.empty() ? 0 : ipv4_match[0].length()));
        person_tokens += item.sample.find("{{PERSON}}") != std::string::npos ? 1 : 0;
        store_tokens += item.sample.find("{{STORE}}") != std::string::npos ? 1 : 0;
        const auto analysis = application::LogRenderer::analyze(item.sample);
        timestamp_templates += analysis.timestamp_count > 0 ? 1 : 0;
        source_ip_templates += analysis.source_ip_count > 0 ? 1 : 0;
        destination_ip_templates += analysis.destination_ip_count > 0 ? 1 : 0;
        privacy_templates += analysis.privacy_token_count > 0 ? 1 : 0;
    }
    expect(timestamp_templates >= 40, "Too few CSV templates have recognized timestamps");
    expect(source_ip_templates >= 10, "Too few CSV templates have recognized source IP fields");
    expect(destination_ip_templates >= 10, "Too few CSV templates have recognized destination IP fields");
    expect(privacy_templates >= 50, "Too few CSV templates have privacy tokens");
    expect(person_tokens >= 10 && store_tokens >= 3, "Person or store anonymization coverage is too low");
    for (const auto index : {std::size_t{79}, std::size_t{82}}) {
        const auto& item = items[index];
        expect(item.sample.find("{{PERSON}}") != std::string::npos, "Position-based person anonymization is missing in " + item.id);
        expect(item.sample.find("{{USER_ID}}") != std::string::npos, "Position-based account anonymization is missing in " + item.id);
        expect(item.sample.find("{{DEPARTMENT}}") != std::string::npos, "Position-based department anonymization is missing in " + item.id);
        expect(item.sample.find("{{HOST}}") != std::string::npos, "Position-based host anonymization is missing in " + item.id);
        expect(item.sample.find("{{SRC_IP}}") != std::string::npos, "Position-based source IP anonymization is missing in " + item.id);
    }

    const auto directory = std::filesystem::current_path() / (".test_json_catalog_" + std::to_string(GetCurrentProcessId()));
    const auto file = directory / "sample_logs.json";
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    const std::vector<domain::LogTemplate> expected{
        {"custom-1", "사용자 로그", "timestamp=2030-01-02T03:04:05Z src_ip=10.0.0.1 dst_ip=10.0.0.2"},
        {"custom-2", "Multiline", "line one\nline two"},
    };
    infrastructure::JsonLogCatalog temporary_catalog{file};
    temporary_catalog.save(expected);
    const auto actual = temporary_catalog.load();
    expect(actual.size() == expected.size(), "JSON catalog round trip changed item count");
    expect(actual[0].id == expected[0].id, "JSON catalog round trip changed id");
    expect(actual[0].name == expected[0].name, "JSON catalog round trip changed UTF-8 name");
    expect(actual[0].sample == expected[0].sample, "JSON catalog round trip changed sample");
    expect(actual[1].sample == expected[1].sample, "JSON catalog round trip changed multiline sample");
    const std::vector<domain::LogTemplate> empty;
    temporary_catalog.save(empty);
    expect(temporary_catalog.load().empty(), "JSON catalog could not persist an empty catalog");

    const auto blocked_file = directory / "blocked.json";
    std::filesystem::create_directory(blocked_file);
    infrastructure::JsonLogCatalog blocked_catalog{blocked_file};
    bool blocked_save_failed = false;
    try {
        blocked_catalog.save(expected);
    } catch (const std::runtime_error&) {
        blocked_save_failed = true;
    }
    expect(blocked_save_failed, "JSON catalog did not report an atomic replacement failure");
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        expect(!entry.path().filename().string().starts_with("blocked.json.tmp."), "JSON catalog left a failed temporary file behind");
    }
    std::filesystem::remove_all(directory, cleanup_error);
}

}
