// tests/log_renderer_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

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

    auto static_log = application::LogRenderer::prepare_one("static src_ip=1.1.1.1", "10.0.0.1", "10.0.0.2", seconds{0});
    auto static_copy = static_log;
    expect(static_copy.render(base_time) == "static src_ip=10.0.0.1", "Static prepared log copy failed");
}

}
