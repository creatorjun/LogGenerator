// src/domain/generator_config.hpp
#pragma once

#include "domain/log_template.hpp"
#include "domain/protocol.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
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
        const auto first = start.time_since_epoch().count();
        const auto last = end.time_since_epoch().count();
        std::uint64_t difference = 0;
        if (first < 0 && last >= 0) {
            const auto negative = static_cast<std::uint64_t>(-(first + 1)) + 1;
            const auto positive = static_cast<std::uint64_t>(last);
            if (negative > std::numeric_limits<std::uint64_t>::max() - positive) {
                return 0;
            }
            difference = negative + positive;
        } else {
            difference = static_cast<std::uint64_t>(last - first);
        }
        if (difference == std::numeric_limits<std::uint64_t>::max()) {
            return 0;
        }
        return difference + 1;
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
