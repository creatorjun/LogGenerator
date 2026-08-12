// tests/log_renderer_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "application/privacy_anonymizer.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

namespace loggen::tests {

void run_log_renderer_tests() {
    using namespace std::chrono;
    const auto base_time = sys_days{year{2030} / January / 2} + hours{3} + minutes{4} + seconds{5};

    auto json = application::LogRenderer::prepare_one(
        R"({"timestamp":"2025-07-10T07:20:00Z","srcip":"10.10.10.10","dstip":"192.168.1.5"})",
        "172.16.1.10",
        "172.16.1.20",
        hours{2});
    const std::string json_result{json.render(base_time)};
    expect(json_result.find("2030-01-02T05:04:05Z") != std::string::npos, "ISO timestamp replacement failed");
    expect(json_result.find("\"srcip\":\"172.16.1.10\"") != std::string::npos, "JSON source IP replacement failed");
    expect(json_result.find("\"dstip\":\"172.16.1.20\"") != std::string::npos, "JSON destination IP replacement failed");

    auto key_value = application::LogRenderer::prepare_one(
        "stime='2024-01-04 17:05:15' src_ip=192.168.1.171 dstn_ip=1.163.55.175",
        "10.1.1.1",
        "10.2.2.2",
        seconds{0});
    const std::string key_value_result{key_value.render(base_time)};
    expect(key_value_result.find("2030-") != std::string::npos && key_value_result.find("2024-01-04") == std::string::npos, "Year-first timestamp replacement failed");
    expect(key_value_result.find("src_ip=10.1.1.1") != std::string::npos, "Key-value source IP replacement failed");
    expect(key_value_result.find("dstn_ip=10.2.2.2") != std::string::npos, "Key-value destination IP replacement failed");

    auto arrow = application::LogRenderer::prepare_one(
        "Apr 20 22:23:19 2021 alert 192.168.85.76:49278 -> 192.168.1.133:60001",
        "10.10.0.1",
        "10.10.0.2",
        seconds{0});
    const std::string arrow_result{arrow.render(base_time)};
    expect(arrow_result.find("2030") != std::string::npos && arrow_result.find("2021") == std::string::npos, "Syslog timestamp replacement failed");
    expect(arrow_result.find("10.10.0.1:49278 -> 10.10.0.2:60001") != std::string::npos, "Arrow IP replacement failed");

    auto weekday = application::LogRenderer::prepare_one(
        "Tue Apr 20 22:23:19 2021 event",
        "10.0.0.1",
        "10.0.0.2",
        seconds{0});
    expect(weekday.render(base_time, true).find("Wed Jan 02 03:04:05 2030") != std::string::npos, "Weekday timestamp replacement is inconsistent");

    const std::string cached_first{arrow.render(base_time + milliseconds{100})};
    const std::string cached_second{arrow.render(base_time + milliseconds{900})};
    expect(cached_first == cached_second, "Per-second render cache is inconsistent");

    auto month_first = application::LogRenderer::prepare_one(
        "event_time=Jul 05 2025 13:53:53 UTC",
        "10.0.0.1",
        "10.0.0.2",
        hours{2});
    const std::string month_first_result{month_first.render(base_time)};
    expect(month_first_result.find("Jan 02 2030 05:04:05 UTC") != std::string::npos, "Month-first UTC timestamp replacement failed");

    auto separated = application::LogRenderer::prepare_one(
        "date=2025/07/05 time=13:53:53.123 src-ip=192.0.2.1 destination-address=192.0.2.2",
        "198.51.100.1",
        "198.51.100.2",
        hours{2});
    const auto adjusted_time = system_clock::to_time_t(base_time + hours{2});
    std::tm adjusted_local{};
    localtime_s(&adjusted_local, &adjusted_time);
    char separated_expected[48]{};
    std::snprintf(
        separated_expected,
        sizeof(separated_expected),
        "date=%04d/%02d/%02d time=%02d:%02d:%02d.000",
        adjusted_local.tm_year + 1900,
        adjusted_local.tm_mon + 1,
        adjusted_local.tm_mday,
        adjusted_local.tm_hour,
        adjusted_local.tm_min,
        adjusted_local.tm_sec);
    const std::string separated_result{separated.render(base_time)};
    expect(separated_result.find(separated_expected) != std::string::npos, "Separated date and time replacement failed");
    expect(separated_result.find("src-ip=198.51.100.1") != std::string::npos, "Hyphenated source IP replacement failed");
    expect(separated_result.find("destination-address=198.51.100.2") != std::string::npos, "Hyphenated destination IP replacement failed");

    const auto analysis = application::LogRenderer::analyze(
        "Jul 05 2025 13:53:53 UTC date=2025-07-05 time=13:53:53 srp_ip=1.1.1.1 src-ip=1.1.1.2 clientipaddr=1.1.1.3 dest_ip=2.2.2.1 destination-address=2.2.2.2 dstn_ip=2.2.2.3");
    expect(analysis.timestamp_count == 3, "Timestamp analysis count is incorrect");
    expect(analysis.source_ip_count == 3, "Source IP analysis count is incorrect");
    expect(analysis.destination_ip_count == 3, "Destination IP analysis count is incorrect");

    const auto privacy_analysis = application::LogRenderer::analyze("{{PERSON}} {{STORE}} {{USER_ID}}");
    expect(privacy_analysis.privacy_token_count == 3, "Privacy token analysis count is incorrect");
    expect((privacy_analysis.privacy_token_mask & application::privacy_token_bit(application::PrivacyTokenKind::Person)) != 0, "Person category mask is missing");
    expect((privacy_analysis.privacy_token_mask & application::privacy_token_bit(application::PrivacyTokenKind::Store)) != 0, "Store category mask is missing");
    expect(application::PrivacyAnonymizer::search_terms(application::PrivacyTokenKind::Person).find("이름") != std::string_view::npos, "Privacy search metadata is missing");

    auto static_log = application::LogRenderer::prepare_one("static src_ip=1.1.1.1", "10.0.0.1", "10.0.0.2", seconds{0});
    auto static_copy = static_log;
    expect(static_copy.render(base_time) == "static src_ip=10.0.0.1", "Static prepared log copy failed");

    auto calendar_log = application::LogRenderer::prepare_one("timestamp=2025-07-05T13:53:53+09:00", "10.0.0.1", "10.0.0.2", seconds{0});
    const auto calendar_start = sys_days{year{2026} / January / 1};
    expect(calendar_log.render(calendar_start, true) == "timestamp=2026-01-01T00:00:00+09:00", "Calendar range rendering applied an unwanted timezone shift");

    const auto sanitized = application::PrivacyAnonymizer::sanitize(
        "vendor=lottermart alternate=lottemart store_name=당진점 user_name=김테스트 user_id=real-user emp_no=991122 email=person@example.com phone=010-1234-5678 remote_ip=10.20.30.40 src_ip=10.0.0.10 dst_ip=10.0.0.20 hmac=abcdef");
    expect(sanitized.find("lottermart") == std::string::npos, "Company token was not sanitized");
    expect(sanitized.find("Your-Company") != std::string::npos && sanitized.find("Yourcompany") == std::string::npos, "Company token replacement is incorrect");
    expect(sanitized.find("당진점") == std::string::npos && sanitized.find("김테스트") == std::string::npos, "Korean personal data was not removed");
    expect(sanitized.find("{{STORE}}") != std::string::npos && sanitized.find("{{PERSON}}") != std::string::npos, "Store or person marker is missing");
    expect(sanitized.find("{{USER_ID}}") != std::string::npos && sanitized.find("{{EMPLOYEE_ID}}") != std::string::npos, "Account marker is missing");
    expect(sanitized.find("{{EMAIL}}") != std::string::npos && sanitized.find("{{PHONE}}") != std::string::npos, "Contact marker is missing");
    expect(sanitized.find("{{IP_ADDRESS}}") != std::string::npos, "Personal IP marker is missing");
    expect(sanitized.find("src_ip=10.0.0.10") != std::string::npos && sanitized.find("dst_ip=10.0.0.20") != std::string::npos, "Configurable source or destination IP was anonymized too early");
    expect(sanitized.find("hmac=abcdef") != std::string::npos, "HMAC was misclassified as a MAC address");
    expect(application::PrivacyAnonymizer::sanitize(sanitized) == sanitized, "Privacy sanitization is not idempotent");

    auto positioned_ips = application::LogRenderer::prepare_one("{{SRC_IP}}:1000 -> {{DST_IP}}:2000", "192.0.2.10", "192.0.2.20", seconds{0});
    expect(positioned_ips.render(base_time) == "192.0.2.10:1000 -> 192.0.2.20:2000", "Position-based source or destination IP marker failed");

    auto privacy_log = application::LogRenderer::prepare_one(sanitized, "192.0.2.10", "192.0.2.20", seconds{0});
    const std::regex person_pattern{R"(user_name=홍길동 ([1-4][0-9]|50|[1-9]))", std::regex::ECMAScript};
    const std::regex store_pattern{R"(store_name=([1-4][0-9]|50|[1-9])호점)", std::regex::ECMAScript};
    std::unordered_set<std::string> generated_people;
    for (int index = 0; index < 200; ++index) {
        const std::string rendered{privacy_log.render(base_time)};
        std::smatch person_match;
        expect(std::regex_search(rendered, person_match, person_pattern), "Generated person is outside the 1-50 range");
        expect(std::regex_search(rendered, store_pattern), "Generated store is outside the 1-50 range");
        expect(rendered.find("{{") == std::string::npos, "Privacy marker leaked into a rendered log");
        expect(rendered.find("src_ip=192.0.2.10") != std::string::npos && rendered.find("dst_ip=192.0.2.20") != std::string::npos, "Source or destination IP rendering regressed");
        generated_people.insert(person_match[1].str());
    }
    expect(generated_people.size() >= 10, "Generated personal data is not sufficiently randomized");

    std::string all_markers;
    for (const auto kind : application::privacy_token_kinds) {
        all_markers.append(std::to_string(static_cast<unsigned int>(kind)));
        all_markers.push_back('=');
        all_markers.append(application::PrivacyAnonymizer::marker(kind));
        all_markers.push_back('|');
    }
    auto correlated_log = application::LogRenderer::prepare_one(std::move(all_markers), "192.0.2.10", "192.0.2.20", seconds{0});
    const std::string correlated_result{correlated_log.render(base_time)};
    std::smatch correlated_person;
    expect(std::regex_search(correlated_result, correlated_person, std::regex{R"(홍길동 ([1-4][0-9]|50|[1-9]))"}), "Synthetic profile person is invalid");
    const auto profile_index = static_cast<std::size_t>(std::stoul(correlated_person[1].str()) - 1);
    for (const auto kind : application::privacy_token_kinds) {
        std::string expected{std::to_string(static_cast<unsigned int>(kind))};
        expected.push_back('=');
        expected.append(application::PrivacyAnonymizer::synthetic_value(kind, profile_index));
        expected.push_back('|');
        expect(correlated_result.find(expected) != std::string::npos, "Synthetic profile fields are not correlated");
    }
    std::unordered_set<std::string> unique_accounts;
    for (std::size_t index = 0; index < application::PrivacyAnonymizer::synthetic_profile_count; ++index) {
        unique_accounts.emplace(application::PrivacyAnonymizer::synthetic_value(application::PrivacyTokenKind::UserId, index));
    }
    expect(unique_accounts.size() == application::PrivacyAnonymizer::synthetic_profile_count, "Synthetic profile accounts are not unique");
}

}
