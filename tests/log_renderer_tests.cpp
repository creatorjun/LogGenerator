// tests/log_renderer_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"
#include "application/privacy_anonymizer.hpp"

#include <nlohmann/json.hpp>

#include <array>
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
    const std::tm adjusted_local = *std::localtime(&adjusted_time);
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

    auto extended_dates = application::LogRenderer::prepare_one(
        "minute=2025-12-03 1:17 file=20260224__transaction monthly=202512_archive access_hms=\"174130\" compact=20251203T015908.232 checkmonth=2025-11",
        "10.0.0.1",
        "10.0.0.2",
        seconds{0});
    const std::string extended_date_result{extended_dates.render(base_time, true)};
    expect(extended_date_result.find("minute=2030-01-02 3:04") != std::string::npos, "Minute timestamp replacement failed");
    expect(extended_date_result.find("file=20300102__transaction") != std::string::npos, "Compact filename date replacement failed");
    expect(extended_date_result.find("monthly=203001_archive") != std::string::npos, "Compact filename month replacement failed");
    expect(extended_date_result.find("access_hms=\"030405\"") != std::string::npos, "Compact time replacement failed");
    expect(extended_date_result.find("compact=20300102T030405.000") != std::string::npos, "Compact T timestamp replacement failed");
    expect(extended_date_result.find("checkmonth=2030-01") != std::string::npos, "Year-month replacement failed");

    auto delimited_date = application::LogRenderer::prepare_one(
        "3`20260430`09:45:34`metrics`108136744`114468928` decide_time=\"0000-00-00 00:00:00\"",
        "10.0.0.1",
        "10.0.0.2",
        seconds{0});
    const std::string delimited_date_result{delimited_date.render(base_time, true)};
    expect(delimited_date_result.find("`20300102`03:04:05`") != std::string::npos, "Delimited date and time replacement failed");
    expect(delimited_date_result.find("decide_time=\"0000-00-00 00:00:00\"") != std::string::npos, "Zero-date sentinel was modified");

    const std::array timestamp_samples{
        std::string{"timestamp=2025-07-10T07:20:00.123Z"},
        std::string{"compact=20251203T015908.232+0900"},
        std::string{"event_time=Jul 05 2025 13:53:53 UTC"},
        std::string{"event_time=Apr 20 22:23:19 2021"},
        std::string{"access=10/Oct/2000:13:55:36 -0700"},
        std::string{"syslog=Apr 20 22:23:19"},
        std::string{"compact=20251203174030"},
        std::string{"minute=2025-12-03 1:17"},
        std::string{"file=20260224__transaction"},
        std::string{"monthly=202512_archive"},
        std::string{"access_hms=174130"},
        std::string{"date=2025/07/05"},
        std::string{"checkmonth=2025-11"},
        std::string{"time=13:53:53.123"},
        std::string{"record`09:45:34`tail"},
    };
    for (const auto& raw_timestamp_sample : timestamp_samples) {
        const auto tokenized_timestamp_sample = application::LogRenderer::tokenize(raw_timestamp_sample);
        expect(tokenized_timestamp_sample.find("{{TIMESTAMP:") != std::string::npos, "A supported timestamp format was not converted to a persistent token");
        expect(application::LogRenderer::tokenize(tokenized_timestamp_sample) == tokenized_timestamp_sample, "Persistent timestamp tokenization is not idempotent");
        auto raw_timestamp_log = application::LogRenderer::prepare_one(raw_timestamp_sample, "10.0.0.1", "10.0.0.2", hours{7});
        auto persisted_timestamp_log = application::LogRenderer::prepare_one(tokenized_timestamp_sample, "10.0.0.1", "10.0.0.2", hours{7});
        expect(raw_timestamp_log.render(base_time) == persisted_timestamp_log.render(base_time), "A persisted timestamp token changed its format or offset behavior");
    }

    const auto analysis = application::LogRenderer::analyze(
        "Jul 05 2025 13:53:53 UTC date=2025-07-05 time=13:53:53 srp_ip=1.1.1.1 src-ip=1.1.1.2 clientipaddr=1.1.1.3 dest_ip=2.2.2.1 destination-address=2.2.2.2 dstn_ip=2.2.2.3");
    expect(analysis.timestamp_count == 3, "Timestamp analysis count is incorrect");
    expect(analysis.source_ip_count == 3, "Source IP analysis count is incorrect");
    expect(analysis.destination_ip_count == 3, "Destination IP analysis count is incorrect");

    const auto privacy_analysis = application::LogRenderer::analyze("{{PERSON}} {{STORE}} {{STORE_CODE}} {{USER_ID}}");
    expect(privacy_analysis.privacy_token_count == 4, "Privacy token analysis count is incorrect");
    expect((privacy_analysis.privacy_token_mask & application::privacy_token_bit(application::PrivacyTokenKind::Person)) != 0, "Person category mask is missing");
    expect((privacy_analysis.privacy_token_mask & application::privacy_token_bit(application::PrivacyTokenKind::Store)) != 0, "Store category mask is missing");
    expect((privacy_analysis.privacy_token_mask & application::privacy_token_bit(application::PrivacyTokenKind::StoreCode)) != 0, "Store code category mask is missing");
    expect(application::PrivacyAnonymizer::search_terms(application::PrivacyTokenKind::Person).find("이름") != std::string_view::npos, "Privacy search metadata is missing");

    auto static_log = application::LogRenderer::prepare_one("static src_ip=1.1.1.1", "10.0.0.1", "10.0.0.2", seconds{0});
    auto static_copy = static_log;
    expect(static_copy.render(base_time) == "static src_ip=10.0.0.1", "Static prepared log copy failed");

    auto calendar_log = application::LogRenderer::prepare_one("timestamp=2025-07-05T13:53:53+09:00", "10.0.0.1", "10.0.0.2", seconds{0});
    const auto calendar_start = sys_days{year{2026} / January / 1};
    expect(calendar_log.render(calendar_start, true) == "timestamp=2026-01-01T00:00:00+09:00", "Calendar range rendering applied an unwanted timezone shift");

    const auto sanitized = application::PrivacyAnonymizer::sanitize(
        "vendor=lottermart alternate=lottemart company=test123 store_name=당진점 user_name=김테스트 user_id=real-user emp_no=991122 email=person@example.com phone=010-1234-5678 remote_ip=10.20.30.40 src_ip=10.0.0.10 dst_ip=10.0.0.20 hmac=abcdef");
    expect(sanitized.find("lottermart") == std::string::npos, "Company token was not sanitized");
    expect(sanitized.find("Your-Company") != std::string::npos && sanitized.find("Yourcompany") == std::string::npos, "Company token replacement is incorrect");
    expect(sanitized.find("당진점") == std::string::npos && sanitized.find("김테스트") == std::string::npos, "Korean personal data was not removed");
    expect(sanitized.find("{{STORE}}") != std::string::npos && sanitized.find("{{PERSON}}") != std::string::npos, "Store or person marker is missing");
    expect(sanitized.find("{{USER_ID}}") != std::string::npos && sanitized.find("{{EMPLOYEE_ID}}") != std::string::npos, "Account marker is missing");
    expect(sanitized.find("{{EMAIL}}") != std::string::npos && sanitized.find("{{PHONE}}") != std::string::npos, "Contact marker is missing");
    expect(sanitized.find("{{IP_ADDRESS}}") != std::string::npos, "Personal IP marker is missing");
    expect(sanitized.find("src_ip=10.0.0.10") != std::string::npos && sanitized.find("dst_ip=10.0.0.20") != std::string::npos, "Configurable source or destination IP was anonymized too early");
    expect(sanitized.find("test123") == std::string::npos, "The revised source company placeholder was not normalized");
    expect(sanitized.find("hmac={{SECRET}}") != std::string::npos, "HMAC was not classified as a secret");
    expect(application::PrivacyAnonymizer::sanitize(sanitized) == sanitized, "Privacy sanitization is not idempotent");
    const auto metric_sanitized = application::PrivacyAnonymizer::sanitize("metrics=108136744,114468928,1092710530,1633337448");
    expect(metric_sanitized.find("{{PHONE}}") == std::string::npos, "System metrics were misclassified as phone numbers");
    expect(application::PrivacyAnonymizer::sanitize("mobile=+82-10-1234-5678").find("{{PHONE}}") != std::string::npos, "International mobile number was not anonymized");
    expect(application::PrivacyAnonymizer::sanitize("mac=\"B8:\"{{MAC_ADDRESS}}\"") == "mac=\"{{MAC_ADDRESS}}\"", "A partial MAC marker was not normalized");
    const std::string mac_key_sample = R"privacy(db.log.deleteMany({mac:"{{MAC_ADDRESS}}"})db.log.deleteMany({mac: "{{MAC_ADDRESS}}"}))privacy";
    expect(application::PrivacyAnonymizer::sanitize(mac_key_sample) == mac_key_sample, "A MAC field name was misclassified as a partial MAC address");
    expect(application::PrivacyAnonymizer::sanitize(std::string{"value="} + '\b') == "value=\\b", "A backspace control character was not escaped");

    const auto revised_fields = application::PrivacyAnonymizer::sanitize(
        R"(ldap_name="정성태" ldap_sno="1120317" ldap_tel="단품관리팀" sess_uname="마트GO_조인기" sess_localid="joinki" sess_domain="db.internal" sess_safpath="/data/private/1" payload="encoded" principalid="AIDAEXAMPLE")");
    expect(revised_fields.find("ldap_name=\"{{PERSON}}\"") != std::string::npos && revised_fields.find("sess_uname=\"{{PERSON}}\"") != std::string::npos, "Revised person fields were not classified");
    expect(revised_fields.find("ldap_sno=\"{{USER_ID}}\"") != std::string::npos && revised_fields.find("sess_localid=\"{{USER_ID}}\"") != std::string::npos, "Revised account fields were not classified");
    expect(revised_fields.find("ldap_tel=\"{{DEPARTMENT}}\"") != std::string::npos, "Revised department field was not classified");
    expect(revised_fields.find("sess_domain=\"{{HOST}}\"") != std::string::npos && revised_fields.find("sess_safpath=\"{{FILE_PATH}}\"") != std::string::npos, "Revised host or path field was not classified");
    expect(revised_fields.find("payload=\"{{SECRET}}\"") != std::string::npos && revised_fields.find("principalid=\"{{IDENTIFIER}}\"") != std::string::npos, "Revised secret or identifier field was not classified");

    const auto store_sanitized = application::PrivacyAnonymizer::sanitize("str_cd=2201 str_nm=당진점 pspsa.str_cd = {{STORE}}");
    expect(store_sanitized.find("str_cd={{STORE_CODE}}") != std::string::npos, "Store code field was not classified separately");
    expect(store_sanitized.find("str_nm={{STORE}}") != std::string::npos, "Store name field was not classified as a display name");
    expect(store_sanitized.find("pspsa.str_cd = {{STORE_CODE}}") != std::string::npos, "Legacy store marker was not upgraded in a code field");
    auto store_log = application::LogRenderer::prepare_one(store_sanitized, "192.0.2.10", "192.0.2.20", seconds{0});
    const std::string store_result{store_log.render(base_time)};
    expect(std::regex_search(store_result, std::regex{R"(str_cd=([1-4][0-9]|50|[1-9])\b)"}), "Generated store code is not numeric");
    expect(std::regex_search(store_result, std::regex{R"(str_nm=([1-4][0-9]|50|[1-9])호점)"}), "Generated store display name is invalid");

    auto json_privacy = application::LogRenderer::prepare_one(R"({"path":"{{FILE_PATH}}","name":"{{PERSON}}"})", "192.0.2.10", "192.0.2.20", seconds{0});
    const std::string json_privacy_result{json_privacy.render(base_time)};
    const auto parsed_privacy_json = nlohmann::json::parse(json_privacy_result);
    expect(parsed_privacy_json.at("path").get<std::string>().starts_with("C:/ProgramData/Your-Company/SecurityData/"), "Generated fallback file path is not JSON-safe");

    domain::LogTemplate mapped_test_case{"mapped", "Mapped", R"({"path":"{{FILE_PATH}}","name":"{{PERSON}}"})", "", {}};
    mapped_test_case.test_case.values["FILE_PATH"] = {"C:/Program Files/Your-Company/Agent/agent.exe"};
    auto mapped_log = application::LogRenderer::prepare_one(mapped_test_case, "192.0.2.10", "192.0.2.20", seconds{0});
    const auto mapped_json = nlohmann::json::parse(mapped_log.render(base_time));
    expect(mapped_json.at("path") == "C:/Program Files/Your-Company/Agent/agent.exe", "Log test case value was not mapped exactly");
    mapped_test_case.test_case.values["FILE_PATH"].push_back("unexpected.dat");
    bool mismatched_test_case_rejected = false;
    try {
        static_cast<void>(application::LogRenderer::prepare_one(mapped_test_case, "192.0.2.10", "192.0.2.20", seconds{0}));
    } catch (const std::invalid_argument&) {
        mismatched_test_case_rejected = true;
    }
    expect(mismatched_test_case_rejected, "A mismatched log test case was not rejected");

    auto positioned_ips = application::LogRenderer::prepare_one("{{SRC_IP}}:1000 -> {{DST_IP}}:2000", "192.0.2.10", "192.0.2.20", seconds{0});
    expect(positioned_ips.render(base_time) == "192.0.2.10:1000 -> 192.0.2.20:2000", "Position-based source or destination IP marker failed");
    auto gateway_ip = application::LogRenderer::prepare_one("gateway=10.61.1.31", "192.0.2.10", "192.0.2.20", seconds{0});
    expect(gateway_ip.render(base_time) == "gateway=192.0.2.10", "Raw gateway IP was not mapped to the configured source IP");

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
