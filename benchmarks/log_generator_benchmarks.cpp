#include "application/log_renderer.hpp"
#include "application/ports/execution_runtime.hpp"
#include "application/ports/logger.hpp"
#include "application/ports/log_transport.hpp"
#include "application/stress_test_service.hpp"
#include "domain/generator_config.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

class NullLease final : public loggen::application::IExecutionLease {
};

class BenchmarkRuntime final : public loggen::application::IExecutionRuntime {
public:
    [[nodiscard]] std::unique_ptr<loggen::application::IExecutionLease> acquire_high_resolution_timer() const override {
        return std::make_unique<NullLease>();
    }

    void configure_current_worker() const noexcept override {
    }

    void pause_current_thread() const noexcept override {
        std::this_thread::yield();
    }
};

class NullLogger final : public loggen::application::ILogger {
public:
    void log(loggen::domain::LogLevel, std::string_view) noexcept override {
    }
};

class NullTransport final : public loggen::application::ILogTransport {
public:
    void connect(const loggen::domain::EndpointConfig&) override {
    }

    [[nodiscard]] loggen::application::SendResult send(std::string_view payload) override {
        bytes_ += payload.size();
        return loggen::application::SendResult::Sent;
    }

    [[nodiscard]] bool is_datagram() const noexcept override {
        return false;
    }

private:
    std::uint64_t bytes_{0};
};

class NullTransportFactory final : public loggen::application::ITransportFactory {
public:
    [[nodiscard]] std::unique_ptr<loggen::application::ILogTransport> create(loggen::domain::TransportProtocol) const override {
        return std::make_unique<NullTransport>();
    }
};

loggen::domain::LogTemplate benchmark_template() {
    return {
        "0001",
        "benchmark",
        R"({"timestamp":"2026-01-01T00:00:00.000Z","src_ip":"{{SRC_IP}}","dst_ip":"{{DST_IP}}","user":"{{PERSON}}","account":"{{USER_ID}}","employee":"{{EMPLOYEE_ID}}","department":"{{DEPARTMENT}}","organization":"{{ORGANIZATION}}","email":"{{EMAIL}}","phone":"{{PHONE}}","address":"{{ADDRESS}}","host":"{{HOST}}","identifier":"{{IDENTIFIER}}","secret":"{{SECRET}}","path":"{{FILE_PATH}}"})",
        "benchmark",
        {}};
}

double renderer_benchmark(const std::uint64_t iterations, const bool advance_time, std::uint64_t& checksum) {
    auto prepared = loggen::application::LogRenderer::prepare_one(benchmark_template(), "192.0.2.10", "192.0.2.20", 0s);
    const auto base = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1} + 12h;
    for (std::uint64_t index = 0; index < 100'000; ++index) {
        const auto payload = prepared.render(base, false);
        checksum += payload.size();
    }
    const auto started = std::chrono::steady_clock::now();
    for (std::uint64_t index = 0; index < iterations; ++index) {
        const auto timestamp = advance_time ? base + std::chrono::seconds{index} : base;
        const auto payload = prepared.render(timestamp, advance_time);
        checksum += payload.size();
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return static_cast<double>(iterations) / elapsed;
}

double service_benchmark(const std::uint32_t workers, const std::chrono::milliseconds duration) {
    NullTransportFactory transport_factory;
    BenchmarkRuntime runtime;
    NullLogger logger;
    loggen::application::StressTestService service{transport_factory, runtime, logger};
    loggen::domain::GeneratorConfig config;
    config.endpoint.protocol = loggen::domain::TransportProtocol::Tcp;
    config.templates.push_back(benchmark_template());
    config.worker_count = workers;
    config.target_eps = 0;
    service.start(std::move(config));
    std::this_thread::sleep_for(duration);
    service.stop();
    const auto stats = service.snapshot();
    return stats.elapsed_seconds <= 0.0 ? 0.0 : static_cast<double>(stats.total_messages) / stats.elapsed_seconds;
}

}

int main(int argument_count, char** argument_values) {
    const std::uint64_t iterations = argument_count > 1 ? std::strtoull(argument_values[1], nullptr, 10) : 5'000'000ULL;
    const std::uint32_t maximum_workers = std::max(1U, std::thread::hardware_concurrency());
    std::uint64_t checksum = 0;
    const auto cached_eps = renderer_benchmark(iterations, false, checksum);
    const auto calendar_iterations = std::max<std::uint64_t>(100'000, iterations / 20);
    const auto calendar_eps = renderer_benchmark(calendar_iterations, true, checksum);
    const auto single_worker_eps = service_benchmark(1, 1500ms);
    const auto multi_worker_eps = service_benchmark(maximum_workers, 1500ms);
    std::cout
        << std::format("renderer_cached_eps={:.0f}\n", cached_eps)
        << std::format("renderer_calendar_eps={:.0f}\n", calendar_eps)
        << std::format("service_1_worker_eps={:.0f}\n", single_worker_eps)
        << std::format("service_{}_workers_eps={:.0f}\n", maximum_workers, multi_worker_eps)
        << "checksum=" << checksum << '\n';
}
