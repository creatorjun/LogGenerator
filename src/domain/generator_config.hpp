// src/domain/generator_config.hpp
#pragma once

#include "domain/log_template.hpp"
#include "domain/protocol.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace loggen::domain {

struct EndpointConfig {
    TransportProtocol protocol{TransportProtocol::Udp};
    std::string host{"127.0.0.1"};
    std::uint16_t port{514};
    std::string tls_server_name;
    bool verify_certificate{true};
    StreamFraming framing{StreamFraming::Newline};
    std::string file_output_directory;
    std::uint64_t file_max_total_bytes{0};
    std::uint32_t file_max_count{0};
    std::chrono::milliseconds file_max_duration{0};
};

struct TimeOffset {
    bool negative{false};
    int days{0};
    int hours{0};
    int minutes{0};

    [[nodiscard]] std::chrono::seconds value() const noexcept {
        const auto magnitude = std::chrono::days{days} + std::chrono::hours{hours} + std::chrono::minutes{minutes};
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(magnitude);
        return negative ? -seconds : seconds;
    }
};

enum class TimestampGenerationMode {
    Offset,
    Range
};

struct TimeRange {
    std::chrono::sys_seconds start{};
    std::chrono::sys_seconds end{};

    [[nodiscard]] bool valid() const noexcept {
        return start <= end;
    }

    [[nodiscard]] std::uint64_t inclusive_seconds() const noexcept {
        if (!valid()) {
            return 0;
        }
        return static_cast<std::uint64_t>((end - start).count()) + 1;
    }
};

struct TimestampGeneration {
    TimestampGenerationMode mode{TimestampGenerationMode::Offset};
    TimeOffset offset;
    TimeRange range;
};

struct GeneratorConfig {
    EndpointConfig endpoint;
    std::vector<LogTemplate> templates;
    std::string source_ip{"10.0.0.10"};
    std::string destination_ip{"10.0.0.20"};
    TimestampGeneration timestamp_generation;
    std::uint32_t worker_count{1};
    std::uint64_t target_eps{0};
};

}
