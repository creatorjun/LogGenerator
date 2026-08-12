// tests/stress_test_service_tests.cpp
#include "test_support.hpp"

#include "application/ports/log_transport.hpp"
#include "application/ports/logger.hpp"
#include "application/stress_test_service.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace loggen::tests {
namespace {

struct TransportState {
    std::mutex mutex;
    std::condition_variable condition;
    bool send_entered{false};
    bool release_send{false};
    std::size_t block_after{1};
    std::vector<std::string> payloads;
    std::atomic<std::uint64_t> sends{0};
};

class BlockingDatagramTransport final : public application::ILogTransport {
public:
    explicit BlockingDatagramTransport(std::shared_ptr<TransportState> state)
        : state_(std::move(state)) {
    }

    void connect(const domain::EndpointConfig&) override {
    }

    void send(const std::string_view payload) override {
        {
            std::unique_lock lock(state_->mutex);
            state_->payloads.emplace_back(payload);
            state_->send_entered = true;
            state_->condition.notify_all();
            if (state_->payloads.size() >= state_->block_after) {
                state_->condition.wait(lock, [this] { return state_->release_send; });
            }
        }
        state_->sends.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_datagram() const noexcept override {
        return true;
    }

private:
    std::shared_ptr<TransportState> state_;
};

class BlockingTransportFactory final : public application::ITransportFactory {
public:
    explicit BlockingTransportFactory(std::shared_ptr<TransportState> state)
        : state_(std::move(state)) {
    }

    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(const domain::TransportProtocol) const override {
        return std::make_unique<BlockingDatagramTransport>(state_);
    }

private:
    std::shared_ptr<TransportState> state_;
};

class NullLogger final : public application::ILogger {
public:
    void log(const domain::LogLevel, const std::string_view) noexcept override {
    }
};

}

void run_stress_test_service_tests() {
    using namespace std::chrono;
    auto state = std::make_shared<TransportState>();
    BlockingTransportFactory factory{state};
    NullLogger logger;
    application::StressTestService service{factory, logger};
    domain::GeneratorConfig config;
    config.endpoint.host = "127.0.0.1";
    config.endpoint.port = 5514;
    config.templates.push_back({"test", "test", "timestamp=2025-07-05T13:53:53Z src_ip=1.1.1.1 dst_ip=2.2.2.2", "test"});
    config.worker_count = 1;
    config.target_eps = 0;
    service.start(std::move(config));

    {
        std::unique_lock lock(state->mutex);
        expect(state->condition.wait_for(lock, seconds{2}, [&state] { return state->send_entered; }), "Stress worker did not enter send");
    }
    const auto stop_started = steady_clock::now();
    service.request_stop();
    expect(steady_clock::now() - stop_started < milliseconds{100}, "Non-blocking stop request blocked the caller");
    {
        std::scoped_lock lock(state->mutex);
        state->release_send = true;
    }
    state->condition.notify_all();
    service.stop();
    const auto stats = service.snapshot();
    expect(stats.state == domain::GeneratorState::Stopped, "Stress service did not stop cleanly");
    const auto transport_sends = state->sends.load(std::memory_order_relaxed);
    expect(stats.total_messages >= 1, "Stress service did not count the completed datagram");
    expect(stats.total_messages == transport_sends, std::format("Stress service message count is {}, transport count is {}", stats.total_messages, transport_sends));
    expect(stats.send_errors == 0, "Stress service reported an unexpected send error");

    auto range_state = std::make_shared<TransportState>();
    range_state->block_after = 4;
    BlockingTransportFactory range_factory{range_state};
    application::StressTestService range_service{range_factory, logger};
    domain::GeneratorConfig range_config;
    range_config.endpoint.host = "127.0.0.1";
    range_config.endpoint.port = 5514;
    range_config.templates.push_back({"range", "range", "timestamp=2025-07-05T13:53:53Z", "test"});
    range_config.timestamp_generation.mode = domain::TimestampGenerationMode::Range;
    range_config.timestamp_generation.range.start = sys_days{year{2026} / January / 1};
    range_config.timestamp_generation.range.end = range_config.timestamp_generation.range.start + seconds{2};
    range_config.worker_count = 1;
    range_service.start(std::move(range_config));
    {
        std::unique_lock lock(range_state->mutex);
        expect(range_state->condition.wait_for(lock, seconds{2}, [&range_state] { return range_state->payloads.size() >= range_state->block_after; }), "Timestamp range worker did not generate enough logs");
    }
    range_service.request_stop();
    {
        std::scoped_lock lock(range_state->mutex);
        expect(range_state->payloads[0].find("2026-01-01T00:00:00Z") != std::string::npos, "Timestamp range did not start at the requested date");
        expect(range_state->payloads[1].find("2026-01-01T00:00:01Z") != std::string::npos, "Timestamp range did not advance by one second");
        expect(range_state->payloads[2].find("2026-01-01T00:00:02Z") != std::string::npos, "Timestamp range did not include its end");
        expect(range_state->payloads[3].find("2026-01-01T00:00:00Z") != std::string::npos, "Timestamp range did not cycle at its end");
        range_state->release_send = true;
    }
    range_state->condition.notify_all();
    range_service.stop();
    expect(range_service.snapshot().send_errors == 0, "Timestamp range service reported an unexpected error");
}

}
