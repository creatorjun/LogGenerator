// tests/generator_config_validator_tests.cpp
#include "test_support.hpp"

#include "application/generator_config_validator.hpp"
#include "domain/log_limits.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace loggen::tests {

void run_generator_config_validator_tests() {
    domain::GeneratorConfig config;
    config.templates.push_back({"sample", "Sample", "event src_ip={{SRC_IP}} dst_ip={{DST_IP}}"});
    config.worker_count = 100;
    auto normalized = application::validate_and_normalize(config);
    expect(normalized.worker_count == 64, "Generator worker limit was not normalized");

    config.endpoint.protocol = domain::TransportProtocol::File;
    config.worker_count = 8;
    config.endpoint.framing = domain::StreamFraming::OctetCounting;
    normalized = application::validate_and_normalize(config);
    expect(normalized.worker_count == 1, "FILE protocol did not enforce one writer");
    expect(normalized.endpoint.framing == domain::StreamFraming::Newline, "FILE protocol did not normalize framing");

    bool rejected = false;
    config.source_ip = "999.1.1.1";
    try {
        static_cast<void>(application::validate_and_normalize(std::move(config)));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "Generator use case accepted an invalid source IPv4 address");

    domain::GeneratorConfig oversized;
    oversized.templates.push_back({"oversized", "Oversized", std::string(domain::maximum_log_sample_bytes + 1, 'X')});
    bool oversized_rejected = false;
    try {
        static_cast<void>(application::validate_and_normalize(std::move(oversized)));
    } catch (const std::invalid_argument&) {
        oversized_rejected = true;
    }
    expect(oversized_rejected, "Generator use case accepted an oversized log template");

    domain::GeneratorConfig extreme_range;
    extreme_range.templates.push_back({"range", "Range", "event"});
    extreme_range.timestamp_generation.mode = domain::TimestampGenerationMode::Range;
    extreme_range.timestamp_generation.range.start = std::chrono::sys_seconds{std::chrono::seconds{std::numeric_limits<std::int64_t>::min()}};
    extreme_range.timestamp_generation.range.end = std::chrono::sys_seconds{std::chrono::seconds{std::numeric_limits<std::int64_t>::max()}};
    bool extreme_range_rejected = false;
    try {
        static_cast<void>(application::validate_and_normalize(std::move(extreme_range)));
    } catch (const std::invalid_argument&) {
        extreme_range_rejected = true;
    }
    expect(extreme_range_rejected, "Generator use case accepted an overflowing timestamp range");
}

}
