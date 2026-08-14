// tests/json_log_catalog_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "infrastructure/json_log_catalog.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::tests {
namespace {

std::size_t count_occurrences(const std::string_view input, const std::string_view marker) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = input.find(marker, position)) != std::string_view::npos) {
        ++count;
        position += marker.size();
    }
    return count;
}

}

void run_json_log_catalog_tests() {
    const infrastructure::JsonLogCatalog catalog;
    const auto source = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "sample_logs.json";
    const auto items = catalog.load(source);
    expect(items.size() == 60, "Expected 60 revised Woori POC sample logs");
    expect(items.front().id == "woori-0001", "First revised sample id is incorrect");
    expect(items.back().id == "woori-0060", "Last revised sample id is incorrect");
    expect(items.front().name == "[MONITORAPP]_AIWAF_Traffic_v1_cef_01", "First revised parser name is incorrect");
    expect(items.back().name == "[Somansa]_DB-i_Security_v1_json_09", "Last revised parser name is incorrect");

    std::size_t timestamp_templates = 0;
    std::size_t source_ip_templates = 0;
    std::size_t destination_ip_templates = 0;
    std::size_t privacy_templates = 0;
    std::size_t person_templates = 0;
    std::size_t store_code_templates = 0;
    std::size_t mapped_file_paths = 0;
    const std::array<std::string_view, 31> forbidden_literals{
        "HANBYOUL.K",
        "JH-SON1",
        "LDASOM89",
        "jungtaeyoon",
        "leeeunmi",
        "innjie",
        "joinki",
        "test567",
        "SPls8240505",
        "mfa/galaxy",
        "DESKTOP-VVM4IR1",
        "김한별",
        "손지훈",
        "김다혜",
        "유예원",
        "박세연",
        "최현민",
        "이인지",
        "정성태",
        "이대희",
        "최희재",
        "이다솜",
        "차재영",
        "정태윤",
        "강대묵",
        "조인기",
        "당진점",
        "여천점",
        "원주구곡점",
        "????",
        "�",
    };
    const std::regex ipv4_pattern{R"((^|[^0-9])((?:(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1?[0-9]?[0-9]))([^0-9]|$))", std::regex::ECMAScript};
    const std::regex legacy_store_code_pattern{R"((?:str_cd|site_num|bizpl_cd)\s*[:=]\s*["']?\{\{STORE\}\})", std::regex::ECMAScript | std::regex::icase};
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        char expected_id[16]{};
        std::snprintf(expected_id, sizeof(expected_id), "woori-%04zu", index + 1);
        expect(item.id == expected_id, "Revised sample ids are not sequential");
        expect(!item.name.empty() && !item.sample.empty(), "A revised sample is empty");
        expect(item.source.empty(), "Imported sample source metadata should be omitted");
        expect(item.sample.find_first_of("\r\n") == std::string::npos, "A sample contains an embedded line break in " + item.id);
        expect(std::ranges::none_of(item.sample, [](const unsigned char value) { return value < 0x20U && value != '\t'; }), "A sample contains an unsupported control character in " + item.id);

        std::string lowercase_text = item.name + item.sample;
        std::ranges::transform(lowercase_text, lowercase_text.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
        expect(lowercase_text.find("test123") == std::string::npos, "The source company placeholder remains in " + item.id);
        expect(lowercase_text.find("lotte") == std::string::npos, "A requested company literal remains in " + item.id);
        for (const auto literal : forbidden_literals) {
            expect(item.sample.find(literal) == std::string::npos, "A forbidden personal or corrupted literal remains in " + item.id);
        }

        for (std::sregex_iterator iterator(item.sample.begin(), item.sample.end(), ipv4_pattern), end; iterator != end; ++iterator) {
            const auto& match = *iterator;
            const auto address_position = static_cast<std::size_t>(match.position(2));
            const auto prefix_start = address_position > 16 ? address_position - 16 : 0;
            const auto prefix = std::string_view{item.sample}.substr(prefix_start, address_position - prefix_start);
            expect(prefix.ends_with("Chrome/"), "An original IPv4 address remains in " + item.id);
        }

        const auto file_path_count = count_occurrences(item.sample, "{{FILE_PATH}}");
        if (file_path_count > 0) {
            expect(item.test_case.values.contains("FILE_PATH"), "A FILE_PATH marker is not mapped in " + item.id);
            const auto& values = item.test_case.values.at("FILE_PATH");
            expect(values.size() == file_path_count, "FILE_PATH test case count is incorrect in " + item.id);
            expect(std::ranges::none_of(values, [](const std::string& value) { return value.empty(); }), "An empty FILE_PATH test case remains in " + item.id);
            mapped_file_paths += values.size();
        }

        const auto sanitized_sample = application::PrivacyAnonymizer::sanitize(item.sample);
        expect(!std::regex_search(sanitized_sample, legacy_store_code_pattern), "A display-name marker remains in a store code field in " + item.id);
        const auto analysis = application::LogRenderer::analyze(item);
        timestamp_templates += analysis.timestamp_count > 0 ? 1 : 0;
        source_ip_templates += analysis.source_ip_count > 0 ? 1 : 0;
        destination_ip_templates += analysis.destination_ip_count > 0 ? 1 : 0;
        privacy_templates += analysis.privacy_token_count > 0 ? 1 : 0;
        person_templates += item.sample.find("{{PERSON}}") != std::string::npos ? 1 : 0;
        store_code_templates += sanitized_sample.find("{{STORE_CODE}}") != std::string::npos ? 1 : 0;
    }
    expect(timestamp_templates >= 50, "Too few revised templates have recognized timestamps");
    expect(source_ip_templates >= 20, "Too few revised templates have source IP mappings");
    expect(destination_ip_templates >= 10, "Too few revised templates have destination IP mappings");
    expect(privacy_templates >= 49, "Too few revised templates have privacy mappings");
    expect(person_templates >= 15, "Too few revised templates have person mappings");
    expect(store_code_templates >= 1, "The store code mapping is missing");
    expect(mapped_file_paths == 47, "The revised file-path test case mapping count is incorrect");

    const auto find_item = [&items](const std::string_view id) -> const domain::LogTemplate& {
        const auto iterator = std::ranges::find(items, id, &domain::LogTemplate::id);
        expect(iterator != items.end(), "Required revised catalog item is missing");
        return *iterator;
    };
    expect(application::LogRenderer::analyze(find_item("woori-0008").sample).timestamp_count >= 2, "Secuway timestamps are not recognized");
    expect(application::LogRenderer::analyze(find_item("woori-0027").sample).timestamp_count >= 4, "AIPS compact timestamps are not recognized");
    expect(application::LogRenderer::analyze(find_item("woori-0060")).timestamp_count >= 2, "DB-i filename and transaction timestamps are not recognized");

    const auto validation_time = std::chrono::sys_days{std::chrono::year{2030} / std::chrono::January / 2} + std::chrono::hours{3} + std::chrono::minutes{4} + std::chrono::seconds{5};
    const std::regex stale_separated_date{R"(\b(?:201[89]|202[0-6])[-/](?:0[1-9]|1[0-2])[-/](?:0[1-9]|[12][0-9]|3[01])\b)", std::regex::ECMAScript};
    const std::regex stale_compact_date{R"((^|[^0-9])(?:201[89]|202[0-6])(?:0[1-9]|1[0-2])(?:0[1-9]|[12][0-9]|3[01])(?![0-9]))", std::regex::ECMAScript};
    const std::regex stale_compact_month{R"((^|[^0-9])(?:201[89]|202[0-6])(?:0[1-9]|1[0-2])(?=_))", std::regex::ECMAScript};
    for (const auto& item : items) {
        auto prepared = application::LogRenderer::prepare_one(item, "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        const std::string rendered{prepared.render(validation_time, true)};
        expect(!std::regex_search(rendered, stale_separated_date), "A separated source date was not regenerated in " + item.id);
        expect(!std::regex_search(rendered, stale_compact_date), "A compact source date was not regenerated in " + item.id);
        expect(!std::regex_search(rendered, stale_compact_month), "A compact source month was not regenerated in " + item.id);
        expect(rendered.find("C:/Test/") == std::string::npos && rendered.find("C:\\Test\\") == std::string::npos, "A generic test path was generated in " + item.id);
        expect(rendered.find("C:/ProgramData/Your-Company/SecurityData/event-") == std::string::npos, "A FILE_PATH mapping fell back to the synthetic generator in " + item.id);
        expect(rendered.find("{{") == std::string::npos, "An anonymization marker leaked into generated output in " + item.id);
        expect(rendered.find_first_of("\r\n") == std::string::npos, "A rendered event contains an embedded line break in " + item.id);
        expect(std::ranges::none_of(rendered, [](const unsigned char value) { return value < 0x20U && value != '\t'; }), "A rendered event contains an unsupported control character in " + item.id);
    }

    const auto render_item = [&find_item, validation_time](const std::string_view id) {
        auto prepared = application::LogRenderer::prepare_one(find_item(id), "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        return std::string{prepared.render(validation_time, true)};
    };
    const auto waf = render_item("woori-0003");
    expect(waf.find("|||192.0.2.10|||54141|||192.0.2.20|||443|||") != std::string::npos, "WAF source or destination IP mapping is incorrect");
    expect(waf.find("|||/commonAction.do|||") != std::string::npos && waf.find("POST /commonAction.do HTTP/1.1") != std::string::npos, "WAF path test case was not restored");
    const auto nac_auth = render_item("woori-0024");
    expect(std::regex_search(nac_auth, std::regex{R"(mac="(?:[0-9A-F]{2}:){5}[0-9A-F]{2}")", std::regex::icase}), "Genian NAC MAC address is malformed");
    expect(nac_auth.find("mac=\"B8:\"") == std::string::npos, "A partial Genian NAC MAC address remains");
    expect(render_item("woori-0025").find_first_of("\r\n") == std::string::npos, "Genian NAC audit event was split across lines");
    const auto trusguard_metrics = render_item("woori-0039");
    expect(trusguard_metrics.find("`20300102`03:04:05`") != std::string::npos, "TrusGuard split date or time was not regenerated");
    expect(trusguard_metrics.find("010-0000-") == std::string::npos, "TrusGuard metrics were rendered as phone numbers");
    expect(render_item("woori-0041").find("decide_time=\"0000-00-00 00:00:00\"") != std::string::npos, "DBSafer zero-date sentinel was modified");
    expect(render_item("woori-0044").find("gateway=\"192.0.2.10\"") != std::string::npos, "DBSafer gateway was not mapped to the configured source IP");
    const auto chakra_query = render_item("woori-0056");
    expect(chakra_query.find('\b') == std::string::npos, "ChakraMax query contains a backspace control character");
    expect(count_occurrences(chakra_query, "\\b") == 4, "ChakraMax query separators were not escaped consistently");
    expect(render_item("woori-0010").find(",SIEM,203001_aws_cloudtrail_log.csv,") != std::string::npos, "CloudTrail file test cases are structurally inaccurate");
    expect(render_item("woori-0046").starts_with("/CloudESM/data/dbilog/20300102__policyevaluated_0.json,{"), "DB-i event path was not restored");
    expect(render_item("woori-0049").starts_with("20300102__transaction_0.json,{"), "DB-i transaction filename was not restored");
    expect(render_item("woori-0053").starts_with("20300102__statement_0.json,{"), "DB-i statement filename was not restored");
    expect(render_item("woori-0054").starts_with("db_sess_info_sf_20300102.csv,"), "ChakraMax filename was not restored");
    expect(render_item("woori-0056").starts_with("db_sql_info_gw_20300102.csv,"), "ChakraMax query filename was not restored");
    const auto dbi_session = render_item("woori-0047");
    expect(dbi_session.starts_with("/CloudESM/data/dbilog/20300102__session_0.json,{"), "DB-i session source path was not restored");
    expect(dbi_session.find("/somansa/data/gfs_data/dbi/20300102/default/id-") != std::string::npos, "DB-i nested test case path was not restored");

    for (const auto id : {std::string_view{"woori-0026"}, std::string_view{"woori-0046"}, std::string_view{"woori-0047"}, std::string_view{"woori-0048"}, std::string_view{"woori-0049"}, std::string_view{"woori-0050"}, std::string_view{"woori-0051"}, std::string_view{"woori-0052"}, std::string_view{"woori-0053"}, std::string_view{"woori-0060"}}) {
        const auto rendered = render_item(id);
        const auto json_start = rendered.find('{');
        expect(json_start != std::string::npos, "Expected JSON payload is missing in " + std::string{id});
        const auto parsed = nlohmann::json::parse(rendered.substr(json_start));
        expect(!parsed.is_discarded(), "Generated JSON payload is invalid in " + std::string{id});
    }
    for (const auto id : {std::string_view{"woori-0055"}, std::string_view{"woori-0056"}, std::string_view{"woori-0057"}, std::string_view{"woori-0058"}, std::string_view{"woori-0059"}}) {
        const auto& item = find_item(id);
        expect(item.sample.find("{{PERSON}}") != std::string::npos, "Position-based person mapping is missing in " + std::string{id});
        expect(item.sample.find("{{USER_ID}}") != std::string::npos, "Position-based account mapping is missing in " + std::string{id});
        expect(item.sample.find("{{DEPARTMENT}}") != std::string::npos, "Position-based department mapping is missing in " + std::string{id});
        expect(item.sample.find("{{SRC_IP}}") != std::string::npos, "Position-based source IP mapping is missing in " + std::string{id});
    }

    const auto directory = unique_test_path("loggen_json_catalog_");
    const auto file = directory / "sample_logs.json";
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    std::vector<domain::LogTemplate> expected{
        {"custom-1", "사용자 로그", "timestamp=2030-01-02T03:04:05Z src_ip=10.0.0.1 dst_ip=10.0.0.2", "사용자 정의", {}},
        {"custom-2", "Multiline", "line one\nline two", "사용자 정의", {}},
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
