// benchmarks/stress_engine_benchmark.cpp
#include "application/ports/execution_runtime.hpp"
#include "application/ports/log_transport.hpp"
#include "application/ports/logger.hpp"
#include "application/stress_test_service.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

class NullTransport final : public loggen::application::ILogTransport {
public:
    explicit NullTransport(const bool datagram)
        : datagram_(datagram) {
    }

    void connect(const loggen::domain::EndpointConfig&) override {
    }

    void send(std::string_view) override {
    }

    [[nodiscard]] bool is_datagram() const noexcept override {
        return datagram_;
    }

private:
    bool datagram_{false};
};

class NullTransportFactory final : public loggen::application::ITransportFactory {
public:
    explicit NullTransportFactory(const bool datagram)
        : datagram_(datagram) {
    }

    [[nodiscard]] std::unique_ptr<loggen::application::ILogTransport> create(loggen::domain::TransportProtocol) const override {
        return std::make_unique<NullTransport>(datagram_);
    }

private:
    bool datagram_{false};
};

class NullExecutionLease final : public loggen::application::IExecutionLease {
};

class NullExecutionRuntime final : public loggen::application::IExecutionRuntime {
public:
    [[nodiscard]] std::unique_ptr<loggen::application::IExecutionLease> enable_high_resolution_timing() const override {
        return std::make_unique<NullExecutionLease>();
    }

    void optimize_current_worker() const noexcept override {
    }

    void relax_cpu() const noexcept override {
    }
};

class NullLogger final : public loggen::application::ILogger {
public:
    void log(loggen::application::LogLevel, std::string_view) noexcept override {
    }
};

double run_engine(const bool datagram, const std::uint32_t worker_count) {
    using namespace std::chrono;
    NullTransportFactory factory{datagram};
    NullExecutionRuntime runtime;
    NullLogger logger;
    loggen::application::StressTestService service{factory, runtime, logger};
    loggen::domain::GeneratorConfig config;
    config.endpoint.protocol = datagram ? loggen::domain::TransportProtocol::Udp : loggen::domain::TransportProtocol::Tcp;
    config.endpoint.host = "127.0.0.1";
    config.endpoint.port = 5514;
    config.worker_count = worker_count;
    config.templates.push_back({"benchmark", "benchmark", "timestamp=2025-07-05T13:53:53.123456Z user={{USER_ID}} name={{PERSON}} department={{DEPARTMENT}} host={{HOST}} src_ip={{SRC_IP}} dst_ip={{DST_IP}}"});
    service.start(std::move(config));
    const auto ready_deadline = steady_clock::now() + seconds{2};
    while (service.snapshot().state != loggen::domain::GeneratorState::Running && steady_clock::now() < ready_deadline) {
        std::this_thread::yield();
    }
    if (service.snapshot().state != loggen::domain::GeneratorState::Running) {
        throw std::runtime_error("Benchmark engine did not start");
    }
    const auto started = steady_clock::now();
    std::this_thread::sleep_for(milliseconds{750});
    service.stop();
    const auto elapsed = duration<double>(steady_clock::now() - started).count();
    return static_cast<double>(service.snapshot().total_messages) / elapsed;
}

}

int main() {
    std::cout << "udp_1_worker=" << std::fixed << std::setprecision(0) << run_engine(true, 1) << " events/s\n";
    std::cout << "stream_1_worker=" << std::fixed << std::setprecision(0) << run_engine(false, 1) << " events/s\n";
    std::cout << "stream_4_workers=" << std::fixed << std::setprecision(0) << run_engine(false, 4) << " events/s\n";
    return 0;
}
