// src/application/stress_test_service.hpp
#pragma once

#include "application/log_renderer.hpp"
#include "application/ports/logger.hpp"
#include "application/ports/log_transport.hpp"
#include "domain/transmission_stats.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace loggen::application {

class StressTestService {
public:
    StressTestService(const ITransportFactory& transport_factory, ILogger& logger);
    ~StressTestService();

    StressTestService(const StressTestService&) = delete;
    StressTestService& operator=(const StressTestService&) = delete;

    void start(domain::GeneratorConfig config);
    void request_stop() noexcept;
    void stop() noexcept;
    [[nodiscard]] domain::TransmissionStats snapshot();

private:
    void run_supervisor(domain::GeneratorConfig config, std::stop_token stop_token) noexcept;
    void run_worker(domain::EndpointConfig endpoint, domain::TimestampGeneration timestamp_generation, std::vector<PreparedLog> logs, std::uint64_t quota, std::uint32_t worker_index, std::uint32_t worker_count, std::stop_token stop_token) noexcept;
    void publish_error(std::string message) noexcept;
    void publish_completion(std::string message) noexcept;

    const ITransportFactory& transport_factory_;
    ILogger& logger_;
    std::mutex lifecycle_mutex_;
    std::mutex error_mutex_;
    std::mutex meter_mutex_;
    std::jthread supervisor_;
    std::stop_source stop_source_;
    std::atomic<domain::GeneratorState> state_{domain::GeneratorState::Stopped};
    std::atomic<std::uint64_t> total_messages_{0};
    std::atomic<std::uint64_t> total_bytes_{0};
    std::atomic<std::uint64_t> send_errors_{0};
    std::atomic<std::uint32_t> connected_workers_{0};
    std::chrono::steady_clock::time_point started_at_{};
    std::chrono::steady_clock::time_point meter_at_{};
    std::uint64_t meter_messages_{0};
    double current_eps_{0.0};
    std::string last_error_;
    std::string status_message_;
};

}
