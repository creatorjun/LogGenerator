// tests/log_renderer_tests.cpp
#include "test_support.hpp"

#include "application/log_renderer.hpp"

#include <chrono>
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
}

}
