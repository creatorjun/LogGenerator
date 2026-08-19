// src/application/stress_test_service.hpp
#pragma once

#include "application/log_renderer.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/ports/execution_runtime.hpp"
#include "application/ports/logger.hpp"
#include "application/ports/log_transport.hpp"
#include "application/round_robin_cursor.hpp"
#include "application/use_cases/stress_test.hpp"
#include "domain/transmission_stats.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace loggen::application {

class StressTestService final : public IStressTestUseCase {
public:
    StressTestService(const ITransportFactory& transport_factory, const IExecutionRuntime& execution_runtime, LogPreparationCache& preparation_cache, ILogger& logger);
    ~StressTestService() override;

    StressTestService(const StressTestService&) = delete;
    StressTestService& operator=(const StressTestService&) = delete;

    void start(domain::GeneratorConfig config) override;
    void request_stop() noexcept override;
    void stop() noexcept override;
    [[nodiscard]] domain::TransmissionStats snapshot() override;

private:
    void run_supervisor(domain::GeneratorConfig config, std::uint32_t worker_count, std::stop_token stop_token) noexcept;
    void run_worker(domain::EndpointConfig endpoint, domain::TimestampGeneration timestamp_generation, std::vector<PreparedLog> logs, std::shared_ptr<RoundRobinCursor> round_robin, std::uint64_t quota, std::uint32_t worker_index, std::uint32_t worker_count, std::stop_token stop_token) noexcept;
    void publish_error(std::string message) noexcept;
    void publish_completion(std::string message) noexcept;

    const ITransportFactory& transport_factory_;
    const IExecutionRuntime& execution_runtime_;
    LogPreparationCache& preparation_cache_;
    ILogger& logger_;
    std::mutex lifecycle_mutex_;
    std::mutex error_mutex_;
    std::mutex meter_mutex_;
    std::jthread supervisor_;
    std::stop_source stop_source_;
    std::atomic<domain::GeneratorState> state_{domain::GeneratorState::Stopped};
    std::atomic<std::uint64_t> total_messages_{0};
    std::atomic<std::uint64_t> total_datagrams_{0};
    std::atomic<std::uint64_t> total_bytes_{0};
    std::atomic<std::uint64_t> send_errors_{0};
    std::atomic<std::uint32_t> connected_workers_{0};
    std::atomic<std::uint32_t> active_workers_{0};
    std::atomic<domain::TransmissionMode> transmission_mode_{domain::TransmissionMode::Parallel};
    std::atomic<domain::UdpPacketization> udp_packetization_{domain::UdpPacketization::OneEventPerDatagram};
    std::chrono::steady_clock::time_point started_at_{};
    std::chrono::steady_clock::time_point finished_at_{};
    std::chrono::steady_clock::time_point meter_at_{};
    std::chrono::steady_clock::time_point meter_progress_at_{};
    std::uint64_t meter_messages_{0};
    double meter_idle_seconds_{3.0};
    double current_eps_{0.0};
    std::string last_error_;
    std::string status_message_;
};

}
