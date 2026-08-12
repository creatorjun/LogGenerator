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

struct GeneratorConfig {
    EndpointConfig endpoint;
    std::vector<LogTemplate> templates;
    std::string source_ip{"10.0.0.10"};
    std::string destination_ip{"10.0.0.20"};
    TimeOffset time_offset;
    std::uint32_t worker_count{1};
    std::uint64_t target_eps{0};
};

}
