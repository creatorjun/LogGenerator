// tests/json_log_catalog_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "infrastructure/json_log_catalog.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::tests {

void run_json_log_catalog_tests() {
    const infrastructure::JsonLogCatalog catalog;
    const auto source = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "sample_logs.json";
    const auto items = catalog.load(source);
    expect(items.size() == 84, "Expected 84 CSV replacement sample logs");
    expect(!items.front().id.empty(), "First sample log id is empty");
    expect(!items.front().name.empty(), "First sample log name is empty");
    expect(!items.front().sample.empty(), "First sample log body is empty");
    expect(items.front().id == "csv-0001", "CSV sample ids were not normalized");
    expect(items.front().source.empty(), "CSV sample source should be omitted");

    std::size_t timestamp_templates = 0;
    std::size_t source_ip_templates = 0;
    std::size_t destination_ip_templates = 0;
    std::size_t privacy_templates = 0;
    std::size_t person_tokens = 0;
    std::size_t store_tokens = 0;
    std::size_t store_code_tokens = 0;
    const std::array<std::string_view, 9> forbidden_literals{
        "김다혜",
        "차재영",
        "박효민",
        "leeeunmi",
        "innjie",
        "SPls8240505",
        "mfa/galaxy",
        "????",
        "�",
    };
    const std::regex legacy_store_code_pattern{R"((?:str_cd|site_num|bizpl_cd)\s*[:=]\s*["']?\{\{STORE\}\})", std::regex::ECMAScript | std::regex::icase};
    const std::regex ipv4_pattern{R"(\b(?:(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\b)", std::regex::ECMAScript};
    for (const auto& item : items) {
        expect(item.id.starts_with("csv-"), "An old sample catalog entry remains");
        std::string lowercase_text = item.name + item.sample;
        std::ranges::transform(lowercase_text, lowercase_text.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        expect(lowercase_text.find("lotte") == std::string::npos && lowercase_text.find("mart") == std::string::npos && lowercase_text.find("yourcompany") == std::string::npos, "A requested company token remains");
        expect(item.sample.find("당진점") == std::string::npos, "A real store name remains");
        for (const auto literal : forbidden_literals) {
            expect(item.sample.find(literal) == std::string::npos, "A forbidden personal or corrupted literal remains in " + item.id);
        }
        if (item.sample.find("{{FILE_PATH}}") != std::string::npos) {
            expect(item.test_case.values.contains("FILE_PATH"), "A FILE_PATH marker is not mapped by its log test case in " + item.id);
            expect(!item.test_case.values.at("FILE_PATH").empty(), "A FILE_PATH test case has no values in " + item.id);
        }
        const auto sanitized_sample = application::PrivacyAnonymizer::sanitize(item.sample);
        expect(!std::regex_search(sanitized_sample, legacy_store_code_pattern), "A legacy display-name marker remains in a store code field in " + item.id);
        std::smatch ipv4_match;
        const auto has_ipv4 = std::regex_search(item.sample, ipv4_match, ipv4_pattern);
        expect(!has_ipv4, "An original IPv4 address remains in CSV catalog item " + item.id + " length " + std::to_string(ipv4_match.empty() ? 0 : ipv4_match[0].length()));
        person_tokens += item.sample.find("{{PERSON}}") != std::string::npos ? 1 : 0;
        store_tokens += item.sample.find("{{STORE}}") != std::string::npos ? 1 : 0;
        store_code_tokens += sanitized_sample.find("{{STORE_CODE}}") != std::string::npos ? 1 : 0;
        const auto analysis = application::LogRenderer::analyze(item);
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
    expect(store_code_tokens >= 3, "Store code anonymization coverage is too low");

    const auto find_item = [&items](const std::string_view id) -> const domain::LogTemplate& {
        const auto iterator = std::ranges::find(items, id, &domain::LogTemplate::id);
        expect(iterator != items.end(), "Required catalog item is missing");
        return *iterator;
    };
    expect(application::LogRenderer::analyze(find_item("csv-0009").sample).timestamp_count > 0, "The SSLVPN syslog timestamp was replaced by a host marker");
    expect(application::LogRenderer::analyze(find_item("csv-0012").sample).timestamp_count > 0, "Minute-resolution catalog timestamp is not recognized");
    expect(application::LogRenderer::analyze(find_item("csv-0030").sample).timestamp_count >= 4, "Compact T catalog timestamps are not recognized");
    expect(application::LogRenderer::analyze(find_item("csv-0055").sample).timestamp_count >= 3, "Compact date or time fields are not recognized");
    expect(application::LogRenderer::analyze(find_item("csv-0084").sample).timestamp_count >= 2, "Filename date and transaction timestamp are not both recognized");

    const auto validation_time = std::chrono::sys_days{std::chrono::year{2030} / std::chrono::January / 2} + std::chrono::hours{3} + std::chrono::minutes{4} + std::chrono::seconds{5};
    const std::regex stale_separated_date{R"(\b(?:201[89]|202[1-6])[-/](?:0[1-9]|1[0-2])[-/](?:0[1-9]|[12][0-9]|3[01])\b)", std::regex::ECMAScript};
    const std::regex stale_compact_date{R"((^|[^0-9])(?:201[89]|202[1-6])(?:0[1-9]|1[0-2])(?:0[1-9]|[12][0-9]|3[01])(?![0-9]))", std::regex::ECMAScript};
    const std::regex stale_compact_month{R"((^|[^0-9])(?:201[89]|202[1-6])(?:0[1-9]|1[0-2])(?=_))", std::regex::ECMAScript};
    const std::regex invalid_store_code{R"(str_cd\s*=\s*([1-4][0-9]|50|[1-9])호점)", std::regex::ECMAScript | std::regex::icase};
    for (const auto& item : items) {
        auto prepared = application::LogRenderer::prepare_one(item, "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        const std::string rendered{prepared.render(validation_time, true)};
        expect(!std::regex_search(rendered, stale_separated_date), "A separated source date was not regenerated in " + item.id);
        expect(!std::regex_search(rendered, stale_compact_date), "A compact source date was not regenerated in " + item.id);
        expect(!std::regex_search(rendered, stale_compact_month), "A compact source month was not regenerated in " + item.id);
        expect(rendered.find("C:/Test/") == std::string::npos && rendered.find("C:\\Test\\") == std::string::npos, "A generic test path was generated in " + item.id);
        expect(rendered.find("C:/ProgramData/Your-Company/SecurityData/event-") == std::string::npos, "A test-case path fell back to the generic generator in " + item.id);
        expect(rendered.find("{{") == std::string::npos, "An anonymization marker leaked into generated output in " + item.id);
        expect(!std::regex_search(rendered, invalid_store_code), "A display-name value was inserted into a store code field in " + item.id);
    }
    const auto render_item = [&find_item, validation_time](const std::string_view id) {
        auto prepared = application::LogRenderer::prepare_one(find_item(id), "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        return std::string{prepared.render(validation_time, true)};
    };
    expect(render_item("csv-0002").find("D:/SecurityLab/") != std::string::npos, "Media-control test case path was not mapped");
    expect(render_item("csv-0005").find("POST /catalog-portal/ui/oauth/verify HTTP/1.1") != std::string::npos, "WAF request path was not restored");
    expect(render_item("csv-0012").find(",SIEM,203001_aws_cloudtrail_log.csv,") != std::string::npos, "CloudTrail test case fields are not structurally accurate");
    expect(render_item("csv-0024").find("Your-Companygo-dev-s3-siem,Your-Companygo-dev-log-bucket,SIEM,203001_go_bo_log.csv,") == 0, "GO access test case columns were not restored");
    expect(render_item("csv-0072").starts_with("/CloudESM/data/dbilog/20300102__statement_0.json,{"), "DB-I statement test case was not reconstructed");
    expect(render_item("csv-0077").starts_with("20300102__statement_0.json,{"), "DB-i statement filename was not reconstructed");
    expect(render_item("csv-0078").starts_with("db_sess_info_sf_20300102.csv,"), "CHAKRAMAX test case filename was not restored");
    for (const auto id : {std::string_view{"csv-0051"}, std::string_view{"csv-0070"}, std::string_view{"csv-0071"}, std::string_view{"csv-0072"}, std::string_view{"csv-0077"}, std::string_view{"csv-0084"}}) {
        auto prepared = application::LogRenderer::prepare_one(find_item(id), "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        const std::string rendered{prepared.render(validation_time, true)};
        const auto json_start = rendered.find('{');
        expect(json_start != std::string::npos, "Expected JSON payload is missing in " + std::string{id});
        const auto parsed = nlohmann::json::parse(rendered.substr(json_start));
        expect(!parsed.is_discarded(), "Generated JSON payload is invalid in " + std::string{id});
    }
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
    std::vector<domain::LogTemplate> expected{
        {"custom-1", "사용자 로그", "timestamp=2030-01-02T03:04:05Z src_ip=10.0.0.1 dst_ip=10.0.0.2", "사용자 정의"},
        {"custom-2", "Multiline", "line one\nline two", "사용자 정의"},
    };
    expected[0].test_case.values["FILE_PATH"] = {"C:/ProgramData/Your-Company/example.dat"};
    catalog.save(file, expected);
    const auto actual = catalog.load(file);
    expect(actual.size() == expected.size(), "JSON catalog round trip changed item count");
    expect(actual[0].id == expected[0].id, "JSON catalog round trip changed id");
    expect(actual[0].name == expected[0].name, "JSON catalog round trip changed UTF-8 name");
    expect(actual[0].sample == expected[0].sample, "JSON catalog round trip changed sample");
    expect(actual[1].sample == expected[1].sample, "JSON catalog round trip changed multiline sample");
    expect(actual[0].test_case.values == expected[0].test_case.values, "JSON catalog round trip changed test case values");
    const std::vector<domain::LogTemplate> empty;
    for (int iteration = 0; iteration < 32; ++iteration) {
        catalog.save(file, expected);
    }
    catalog.save(file, empty);
    expect(catalog.load(file).empty(), "JSON catalog could not persist an empty catalog");
    std::filesystem::remove_all(directory, cleanup_error);
}

}
