// src/application/log_renderer.hpp
#pragma once

#include "application/privacy_anonymizer.hpp"
#include "application/log_template_analysis.hpp"
#include "domain/generator_config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

struct TimestampToken {
    TimestampStyle style{TimestampStyle::Iso8601};
    char date_separator{'-'};
    char date_time_separator{'T'};
    std::uint8_t fractional_digits{0};
    int zone_offset_minutes{0};
    bool has_weekday{false};
    std::string zone_suffix;
};

struct RenderSegment {
    std::string text;
    bool is_timestamp{false};
    TimestampToken timestamp;
    PrivacyTokenKind privacy{PrivacyTokenKind::None};
};

class PreparedLog {
public:
    PreparedLog(std::vector<RenderSegment> segments, std::chrono::seconds offset);
    PreparedLog(const PreparedLog& other);
    PreparedLog& operator=(const PreparedLog& other);
    PreparedLog(PreparedLog&& other) noexcept = default;
    PreparedLog& operator=(PreparedLog&& other) noexcept = default;
    [[nodiscard]] std::string_view render(std::chrono::system_clock::time_point now, bool calendar_time = false);
    [[nodiscard]] std::size_t capacity_hint() const noexcept;

private:
    struct CompiledLog {
        std::vector<RenderSegment> segments;
        std::size_t capacity_hint{0};
        bool has_timestamp{false};
        bool has_fractional_timestamp{false};
        bool has_privacy{false};
    };

    void initialize_cache();
    void initialize_random_state() noexcept;
    [[nodiscard]] std::size_t next_profile_index() noexcept;

    std::shared_ptr<const CompiledLog> compiled_;
    std::string cached_;
    std::vector<std::string> timestamp_cache_;
    std::vector<std::size_t> fractional_offsets_;
    std::int64_t cached_second_{-1};
    bool cached_calendar_time_{false};
    bool timestamp_cache_ready_{false};
    std::chrono::seconds offset_{0};
    std::uint64_t random_state_{0};
};

class LogRenderer {
public:
    [[nodiscard]] static LogTemplateAnalysis analyze(std::string_view sample);
    [[nodiscard]] static LogTemplateAnalysis analyze_sanitized(std::string_view sample);
    [[nodiscard]] static std::vector<PreparedLog> prepare(const domain::GeneratorConfig& config);
    [[nodiscard]] static PreparedLog prepare_one(std::string sample, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
};

}
