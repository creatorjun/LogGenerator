// src/application/generator_config_validator.cpp
#include "application/generator_config_validator.hpp"

#include "domain/log_limits.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace loggen::application {
namespace {

bool valid_ipv4(const std::string_view value) {
    int segments = 0;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find('.', start);
        const auto part = value.substr(start, end == std::string_view::npos ? value.size() - start : end - start);
        if (part.empty() || part.size() > 3) {
            return false;
        }
        int number = -1;
        const auto conversion = std::from_chars(part.data(), part.data() + part.size(), number);
        if (conversion.ec != std::errc{} || conversion.ptr != part.data() + part.size() || number < 0 || number > 255) {
            return false;
        }
        ++segments;
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return segments == 4;
}

bool valid_protocol(const domain::TransportProtocol protocol) {
    switch (protocol) {
    case domain::TransportProtocol::Udp:
    case domain::TransportProtocol::Tcp:
    case domain::TransportProtocol::Tls:
    case domain::TransportProtocol::File:
        return true;
    }
    return false;
}

bool valid_framing(const domain::StreamFraming framing) {
    switch (framing) {
    case domain::StreamFraming::Newline:
    case domain::StreamFraming::OctetCounting:
        return true;
    }
    return false;
}

bool valid_timestamp_mode(const domain::TimestampGenerationMode mode) {
    switch (mode) {
    case domain::TimestampGenerationMode::Offset:
    case domain::TimestampGenerationMode::Range:
        return true;
    }
    return false;
}

}

domain::GeneratorConfig validate_and_normalize(domain::GeneratorConfig config) {
    if (!valid_protocol(config.endpoint.protocol)) {
        throw std::invalid_argument("Unsupported transport protocol");
    }
    if (!valid_framing(config.endpoint.framing)) {
        throw std::invalid_argument("Unsupported stream framing");
    }
    if (!valid_timestamp_mode(config.timestamp_generation.mode)) {
        throw std::invalid_argument("Unsupported timestamp generation mode");
    }
    if (config.templates.empty()) {
        throw std::invalid_argument("At least one log template is required");
    }
    if (config.templates.size() > domain::maximum_log_template_count) {
        throw std::invalid_argument("Too many log templates were requested");
    }
    if (std::ranges::any_of(config.templates, [](const domain::LogTemplate& item) { return item.sample.empty(); })) {
        throw std::invalid_argument("Log templates must not be empty");
    }
    if (std::ranges::any_of(config.templates, [](const domain::LogTemplate& item) { return item.sample.size() > domain::maximum_log_sample_bytes; })) {
        throw std::invalid_argument("A log template exceeds the supported size");
    }
    if (!valid_ipv4(config.source_ip) || !valid_ipv4(config.destination_ip)) {
        throw std::invalid_argument("Source and destination addresses must be valid IPv4 addresses");
    }
    if (config.endpoint.protocol != domain::TransportProtocol::File && (config.endpoint.host.empty() || config.endpoint.port == 0)) {
        throw std::invalid_argument("A destination host and port are required");
    }
    if (config.endpoint.host.size() > 253 || config.endpoint.tls_server_name.size() > 253) {
        throw std::invalid_argument("Destination or TLS server name exceeds the supported length");
    }
    if (config.timestamp_generation.mode == domain::TimestampGenerationMode::Range) {
        const auto range_seconds = config.timestamp_generation.range.inclusive_seconds();
        if (!config.timestamp_generation.range.valid() || range_seconds == 0 || range_seconds > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            throw std::invalid_argument("A valid timestamp range is required");
        }
    }
    if (config.timestamp_generation.mode == domain::TimestampGenerationMode::Offset) {
        const auto& offset = config.timestamp_generation.offset;
        if (offset.days < 0 || offset.days > 3650 || offset.hours < 0 || offset.hours > 23 || offset.minutes < 0 || offset.minutes > 59) {
            throw std::invalid_argument("Timestamp offset is outside the supported range");
        }
    }
    config.worker_count = std::clamp<std::uint32_t>(config.worker_count, 1, 64);
    if (config.endpoint.protocol == domain::TransportProtocol::File) {
        config.worker_count = 1;
        config.endpoint.framing = domain::StreamFraming::Newline;
    }
    if (config.target_eps > 0) {
        config.worker_count = static_cast<std::uint32_t>(std::min<std::uint64_t>(config.worker_count, config.target_eps));
    }
    return config;
}

}
