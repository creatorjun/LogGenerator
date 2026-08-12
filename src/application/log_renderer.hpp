// src/application/log_renderer.hpp
#pragma once

#include "domain/generator_config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace loggen::application {

enum class TimestampStyle {
    Iso8601,
    YearFirst,
    SyslogWithYear,
    SyslogWithoutYear,
    MonthFirstGmt,
    Apache,
    Compact
};

struct TimestampToken {
    TimestampStyle style{TimestampStyle::Iso8601};
    char date_separator{'-'};
    char date_time_separator{'T'};
    std::uint8_t fractional_digits{0};
    int zone_offset_minutes{0};
    std::string zone_suffix;
};

struct RenderSegment {
    std::string text;
    bool is_timestamp{false};
    TimestampToken timestamp;
};

class PreparedLog {
public:
    PreparedLog(std::vector<RenderSegment> segments, std::chrono::seconds offset);
    [[nodiscard]] std::string_view render(std::chrono::system_clock::time_point now);
    [[nodiscard]] std::size_t capacity_hint() const noexcept;

private:
    std::vector<RenderSegment> segments_;
    std::string cached_;
    std::int64_t cached_second_{-1};
    std::size_t capacity_hint_{0};
    std::chrono::seconds offset_{0};
};

class LogRenderer {
public:
    [[nodiscard]] static std::vector<PreparedLog> prepare(const domain::GeneratorConfig& config);
    [[nodiscard]] static PreparedLog prepare_one(std::string sample, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
};

}
