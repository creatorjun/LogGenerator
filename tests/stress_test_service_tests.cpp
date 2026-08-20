// tests/stress_test_service_tests.cpp
#include "test_support.hpp"

#include "application/ports/execution_runtime.hpp"
#include "application/ports/log_transport.hpp"
#include "application/ports/logger.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/round_robin_cursor.hpp"
#include "application/stress_test_service.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
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
    bool datagram{true};
};

class BlockingDatagramTransport final : public application::ILogTransport {
public:
    explicit BlockingDatagramTransport(std::shared_ptr<TransportState> state)
        : state_(std::move(state)) {
    }

    void connect(const domain::EndpointConfig&) override {
    }

    [[nodiscard]] application::SendResult send(const std::string_view payload) override {
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
        return application::SendResult::Sent;
    }

    [[nodiscard]] bool is_datagram() const noexcept override {
        return state_->datagram;
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

class LimitingTransport final : public application::ILogTransport {
public:
    void connect(const domain::EndpointConfig&) override {
    }

    [[nodiscard]] application::SendResult send(std::string_view) override {
        return application::SendResult::FileCountLimitReached;
    }

    [[nodiscard]] bool is_datagram() const noexcept override {
        return false;
    }
};

class LimitingTransportFactory final : public application::ITransportFactory {
public:
    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(const domain::TransportProtocol) const override {
        return std::make_unique<LimitingTransport>();
    }
};

class NullLogger final : public application::ILogger {
public:
    void log(const application::LogLevel, const std::string_view) noexcept override {
    }
};

class ExecutionLease final : public application::IExecutionLease {
};

class TestExecutionRuntime final : public application::IExecutionRuntime {
public:
    explicit TestExecutionRuntime(const std::uint32_t optimal_workers = 1) noexcept
        : optimal_workers_(optimal_workers) {
    }

    [[nodiscard]] std::unique_ptr<application::IExecutionLease> acquire_high_resolution_timer() const override {
        return std::make_unique<ExecutionLease>();
    }

    [[nodiscard]] std::uint32_t optimal_worker_count(const domain::TransportProtocol) const noexcept override {
        return optimal_workers_;
    }

    void configure_current_worker() const noexcept override {
        configured_workers_.fetch_add(1, std::memory_order_relaxed);
    }

    void pause_current_thread() const noexcept override {
        std::this_thread::yield();
    }

    [[nodiscard]] std::uint32_t configured_workers() const noexcept {
        return configured_workers_.load(std::memory_order_relaxed);
    }

private:
    std::uint32_t optimal_workers_{1};
    mutable std::atomic<std::uint32_t> configured_workers_{0};
};

}

void run_stress_test_service_tests() {
    using namespace std::chrono;
    application::RoundRobinCursor sequential_cursor{3};
    expect(sequential_cursor.next() == 0, "Global round-robin did not start at the first sample");
    expect(sequential_cursor.next() == 1, "Global round-robin did not advance to the second sample");
    expect(sequential_cursor.next() == 2, "Global round-robin did not advance to the third sample");
    expect(sequential_cursor.next() == 0, "Global round-robin did not wrap to the first sample");

    application::RoundRobinCursor reserved_cursor{3};
    expect(reserved_cursor.reserve(5) == 0, "Global round-robin reservation did not start at the first sample");
    expect(reserved_cursor.next() == 2, "Global round-robin reservation did not advance by the reserved block");

    application::RoundRobinCursor concurrent_cursor{5};
    std::array<std::atomic_size_t, 5> selections{};
    std::vector<std::jthread> cursor_workers;
    for (std::size_t worker = 0; worker < 8; ++worker) {
        cursor_workers.emplace_back([&concurrent_cursor, &selections] {
            for (std::size_t iteration = 0; iteration < 1000; ++iteration) {
                selections[concurrent_cursor.next()].fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& worker : cursor_workers) {
        worker.join();
    }
    for (const auto& selection : selections) {
        expect(selection.load(std::memory_order_relaxed) == 1600, "Global round-robin lost or duplicated a concurrent selection");
    }

    auto state = std::make_shared<TransportState>();
    BlockingTransportFactory factory{state};
    TestExecutionRuntime sequential_runtime{8};
    NullLogger logger;
    application::LogPreparationCache preparation_cache;
    application::StressTestService service{factory, sequential_runtime, preparation_cache, logger};
    domain::GeneratorConfig config;
    config.endpoint.host = "127.0.0.1";
    config.endpoint.port = 5514;
    config.templates.push_back({"test", "test", "timestamp=2025-07-05T13:53:53Z src_ip=1.1.1.1 dst_ip=2.2.2.2", "test", {}});
    config.transmission_mode = domain::TransmissionMode::Sequential;
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
    expect(stats.transmission_mode == domain::TransmissionMode::Sequential && stats.active_workers == 1, "Sequential mode did not force exactly one worker");
    expect(stats.udp_packetization == domain::UdpPacketization::OneEventPerDatagram && stats.total_datagrams == stats.total_messages, "Sequential UDP did not preserve one event per datagram");
    expect(sequential_runtime.configured_workers() == 1, "Sequential mode configured more than one worker");

    auto parallel_state = std::make_shared<TransportState>();
    BlockingTransportFactory parallel_factory{parallel_state};
    TestExecutionRuntime parallel_runtime{3};
    application::StressTestService parallel_service{parallel_factory, parallel_runtime, preparation_cache, logger};
    domain::GeneratorConfig parallel_config;
    parallel_config.endpoint.host = "127.0.0.1";
    parallel_config.endpoint.port = 5514;
    parallel_config.transmission_mode = domain::TransmissionMode::Parallel;
    parallel_config.templates.push_back({"parallel", "parallel", "parallel-event", "test", {}});
    parallel_service.start(std::move(parallel_config));
    const auto parallel_deadline = steady_clock::now() + seconds{2};
    while (parallel_runtime.configured_workers() < 3 && steady_clock::now() < parallel_deadline) {
        std::this_thread::sleep_for(milliseconds{1});
    }
    const auto parallel_stats = parallel_service.snapshot();
    expect(parallel_runtime.configured_workers() == 3, "Parallel mode did not start the runtime-selected worker count");
    expect(parallel_stats.transmission_mode == domain::TransmissionMode::Parallel && parallel_stats.active_workers == 3, "Parallel mode did not publish the auto-selected worker count");
    parallel_service.request_stop();
    {
        std::scoped_lock lock(parallel_state->mutex);
        parallel_state->release_send = true;
    }
    parallel_state->condition.notify_all();
    parallel_service.stop();
    const auto parallel_default_stats = parallel_service.snapshot();
    expect(parallel_default_stats.udp_packetization == domain::UdpPacketization::OneEventPerDatagram, "Parallel UDP enabled integration without explicit permission");
    expect(parallel_default_stats.total_datagrams > 0 && parallel_default_stats.total_messages == parallel_default_stats.total_datagrams, "Default UDP did not preserve one event per datagram");
    expect(parallel_default_stats.total_datagrams == parallel_state->sends.load(std::memory_order_relaxed), "Parallel UDP datagram statistics diverged from transport sends");
    {
        std::scoped_lock lock(parallel_state->mutex);
        expect(!parallel_state->payloads.empty(), "Parallel UDP did not produce a payload");
        expect(parallel_state->payloads.front() == "parallel-event", "Default UDP changed the one-event payload");
    }

    auto integrated_state = std::make_shared<TransportState>();
    BlockingTransportFactory integrated_factory{integrated_state};
    TestExecutionRuntime integrated_runtime;
    application::StressTestService integrated_service{integrated_factory, integrated_runtime, preparation_cache, logger};
    domain::GeneratorConfig integrated_config;
    integrated_config.endpoint.host = "127.0.0.1";
    integrated_config.endpoint.port = 5514;
    integrated_config.endpoint.udp_packetization = domain::UdpPacketization::NewlinePacked;
    integrated_config.transmission_mode = domain::TransmissionMode::Sequential;
    integrated_config.templates.push_back({"integrated", "integrated", std::string(2048, 'x'), "test", {}});
    integrated_service.start(std::move(integrated_config));
    {
        std::unique_lock lock(integrated_state->mutex);
        expect(integrated_state->condition.wait_for(lock, seconds{2}, [&integrated_state] { return integrated_state->send_entered; }), "Integrated UDP worker did not enter send");
    }
    integrated_service.request_stop();
    {
        std::scoped_lock lock(integrated_state->mutex);
        integrated_state->release_send = true;
    }
    integrated_state->condition.notify_all();
    integrated_service.stop();
    const auto integrated_stats = integrated_service.snapshot();
    expect(integrated_stats.udp_packetization == domain::UdpPacketization::NewlinePacked, "Explicit UDP integration permission was not preserved");
    expect(integrated_stats.total_datagrams > 1 && integrated_stats.total_messages > integrated_stats.total_datagrams, "Integrated UDP did not count logical events separately from physical datagrams");
    expect(integrated_stats.total_datagrams == integrated_state->sends.load(std::memory_order_relaxed), "Integrated UDP datagram statistics diverged from transport sends");
    {
        std::scoped_lock lock(integrated_state->mutex);
        std::size_t integrated_bytes = 0;
        for (const auto& payload : integrated_state->payloads) {
            expect(payload.size() <= 60U * 1024U, "Integrated UDP produced a datagram larger than 60 KiB");
            expect(std::count(payload.begin(), payload.end(), '\n') > 1, "Integrated UDP payload did not contain newline-framed events");
            integrated_bytes += payload.size();
        }
        expect(integrated_bytes > 60U * 1024U && integrated_bytes <= 600U * 1024U, "Integrated UDP did not use a bounded 600 KiB transmission unit");
    }

    TestExecutionRuntime execution_runtime;

    auto meter_state = std::make_shared<TransportState>();
    meter_state->block_after = 2;
    meter_state->datagram = false;
    BlockingTransportFactory meter_factory{meter_state};
    application::StressTestService meter_service{meter_factory, execution_runtime, preparation_cache, logger};
    domain::GeneratorConfig meter_config;
    meter_config.endpoint.protocol = domain::TransportProtocol::File;
    meter_config.templates.push_back({"meter", "meter", "meter-event", "test", {}});
    meter_service.start(std::move(meter_config));
    {
        std::unique_lock lock(meter_state->mutex);
        expect(meter_state->condition.wait_for(lock, seconds{2}, [&meter_state] { return meter_state->payloads.size() >= 2; }), "FILE worker did not complete a batch before blocking");
    }
    std::this_thread::sleep_for(milliseconds{250});
    const auto first_meter_stats = meter_service.snapshot();
    expect(first_meter_stats.total_messages >= 256, "FILE batch totals were not published immediately");
    expect(first_meter_stats.current_eps > 0.0, "FILE EPS did not capture a completed batch");
    std::this_thread::sleep_for(milliseconds{250});
    const auto held_meter_stats = meter_service.snapshot();
    expect(held_meter_stats.current_eps > 0.0, "FILE EPS dropped to zero between completed batches");
    meter_service.request_stop();
    {
        std::scoped_lock lock(meter_state->mutex);
        meter_state->release_send = true;
    }
    meter_state->condition.notify_all();
    meter_service.stop();
    expect(meter_service.snapshot().current_eps == 0.0, "Stopped FILE service retained a current EPS value");

    auto range_state = std::make_shared<TransportState>();
    range_state->block_after = 4;
    BlockingTransportFactory range_factory{range_state};
    application::StressTestService range_service{range_factory, execution_runtime, preparation_cache, logger};
    domain::GeneratorConfig range_config;
    range_config.endpoint.host = "127.0.0.1";
    range_config.endpoint.port = 5514;
    range_config.templates.push_back({"range", "range", "timestamp=2025-07-05T13:53:53Z", "test", {}});
    range_config.timestamp_generation.mode = domain::TimestampGenerationMode::Range;
    range_config.timestamp_generation.range.start = sys_days{year{2026} / January / 1};
    range_config.timestamp_generation.range.end = range_config.timestamp_generation.range.start + seconds{2};
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

    auto stream_state = std::make_shared<TransportState>();
    stream_state->datagram = false;
    BlockingTransportFactory stream_factory{stream_state};
    application::StressTestService stream_service{stream_factory, execution_runtime, preparation_cache, logger};
    domain::GeneratorConfig stream_config;
    stream_config.endpoint.protocol = domain::TransportProtocol::File;
    stream_config.templates.push_back({"multiline", "multiline", "alpha\r\nbeta", "test", {}});
    stream_service.start(std::move(stream_config));
    {
        std::unique_lock lock(stream_state->mutex);
        expect(stream_state->condition.wait_for(lock, seconds{2}, [&stream_state] { return stream_state->send_entered; }), "Stream worker did not generate a batch");
    }
    stream_service.request_stop();
    {
        std::scoped_lock lock(stream_state->mutex);
        expect(!stream_state->payloads.empty(), "Stream worker did not capture a payload");
        constexpr std::string_view framed_log{"alpha\\r\\nbeta\n"};
        expect(stream_state->payloads.front().size() > framed_log.size(), "Unlimited FILE mode did not batch multiple logs");
        expect(stream_state->payloads.front().size() % framed_log.size() == 0, "FILE batch ended with a partial log frame");
        for (std::size_t offset = 0; offset < stream_state->payloads.front().size(); offset += framed_log.size()) {
            expect(std::string_view{stream_state->payloads.front()}.substr(offset, framed_log.size()) == framed_log, "FILE batch changed a framed log");
        }
        expect(stream_state->payloads.front().find("alpha\r\nbeta") == std::string::npos, "An embedded physical line break remains in newline framing");
        stream_state->release_send = true;
    }
    stream_state->condition.notify_all();
    stream_service.stop();
    expect(stream_service.snapshot().send_errors == 0, "Stream framing service reported an unexpected error");

    LimitingTransportFactory limiting_factory;
    application::StressTestService limiting_service{limiting_factory, execution_runtime, preparation_cache, logger};
    domain::GeneratorConfig limiting_config;
    limiting_config.endpoint.protocol = domain::TransportProtocol::File;
    limiting_config.templates.push_back({"limit", "limit", "timestamp=2025-07-05T13:53:53Z", "test", {}});
    limiting_service.start(std::move(limiting_config));
    domain::TransmissionStats limiting_stats;
    const auto limit_deadline = steady_clock::now() + seconds{2};
    do {
        limiting_stats = limiting_service.snapshot();
        if (!limiting_stats.status_message.empty()) {
            break;
        }
        std::this_thread::sleep_for(milliseconds{1});
    } while (steady_clock::now() < limit_deadline);
    limiting_service.stop();
    limiting_stats = limiting_service.snapshot();
    expect(!limiting_stats.status_message.empty(), "Configured FILE limit did not publish a completion message");
    expect(limiting_stats.last_error.empty() && limiting_stats.send_errors == 0, "Configured FILE limit was reported as a transmission error");
}

}
