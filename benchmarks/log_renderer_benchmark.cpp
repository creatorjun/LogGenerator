// benchmarks/log_renderer_benchmark.cpp
#include "application/log_renderer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    std::string_view name;
    double events_per_second{0.0};
    std::uint64_t checksum{0};
};

template <typename TimeSource>
BenchmarkResult run_benchmark(const std::string_view name, loggen::application::PreparedLog& prepared, const std::size_t iterations, TimeSource&& time_source) {
    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < 4096; ++index) {
        const auto rendered = prepared.render(time_source(index));
        checksum += rendered.size();
    }
    const auto started = Clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        const auto rendered = prepared.render(time_source(index));
        checksum += rendered.size();
        checksum ^= rendered.empty() ? 0U : static_cast<unsigned char>(rendered[index % rendered.size()]);
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - started).count();
    return {name, static_cast<double>(iterations) / elapsed, checksum};
}

BenchmarkResult run_prepare_benchmark(const std::size_t iterations) {
    constexpr std::string_view sample = "vendor=lottermart timestamp=2025-07-05T13:53:53.123456Z src_ip=192.0.2.1 dst_ip=192.0.2.2 user_name=RealUser user_id=real-user department=Security host_name=REAL-HOST email=user@example.com phone=010-1234-5678";
    std::uint64_t checksum = 0;
    const auto started = Clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        auto prepared = loggen::application::LogRenderer::prepare_one(std::string(sample), "198.51.100.1", "198.51.100.2", std::chrono::seconds{0});
        checksum += prepared.capacity_hint();
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - started).count();
    return {"prepare", static_cast<double>(iterations) / elapsed, checksum};
}

}

int main() {
    using namespace std::chrono;
    constexpr std::size_t iterations = 2'000'000;
    const auto base_time = sys_days{year{2030} / January / 2} + hours{3} + minutes{4} + seconds{5};
    auto static_log = loggen::application::LogRenderer::prepare_one("event=allowed action=read src_ip=192.0.2.1 dst_ip=192.0.2.2", "198.51.100.1", "198.51.100.2", seconds{0});
    auto timestamp_log = loggen::application::LogRenderer::prepare_one("timestamp=2025-07-05T13:53:53Z event=allowed src_ip=192.0.2.1 dst_ip=192.0.2.2", "198.51.100.1", "198.51.100.2", seconds{0});
    auto fractional_log = loggen::application::LogRenderer::prepare_one("timestamp=2025-07-05T13:53:53.123456Z event=allowed src_ip=192.0.2.1 dst_ip=192.0.2.2", "198.51.100.1", "198.51.100.2", seconds{0});
    auto privacy_log = loggen::application::LogRenderer::prepare_one("timestamp=2025-07-05T13:53:53Z user={{USER_ID}} name={{PERSON}} department={{DEPARTMENT}} host={{HOST}} ip={{IP_ADDRESS}}", "198.51.100.1", "198.51.100.2", seconds{0});

    const BenchmarkResult results[]{
        run_benchmark("static", static_log, iterations, [base_time](std::size_t) { return base_time; }),
        run_benchmark("timestamp", timestamp_log, iterations, [base_time](const std::size_t index) { return time_point_cast<system_clock::duration>(base_time + microseconds{index}); }),
        run_benchmark("fractional", fractional_log, iterations, [base_time](const std::size_t index) { return time_point_cast<system_clock::duration>(base_time + nanoseconds{index * 7919ULL}); }),
        run_benchmark("privacy", privacy_log, iterations, [base_time](const std::size_t index) { return time_point_cast<system_clock::duration>(base_time + microseconds{index}); }),
        run_prepare_benchmark(2000),
    };
    for (const auto& result : results) {
        std::cout << result.name << '=' << std::fixed << std::setprecision(0) << result.events_per_second << " events/s checksum=" << result.checksum << '\n';
    }
    return 0;
}
