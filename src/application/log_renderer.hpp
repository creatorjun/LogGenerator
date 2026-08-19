// src/application/log_renderer.hpp
#pragma once

#include "application/models/log_template_analysis.hpp"
#include "application/privacy_anonymizer.hpp"
#include "domain/generator_config.hpp"

#include <array>
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
    std::uint8_t hour_width{2};
    int zone_offset_minutes{0};
    bool has_weekday{false};
    std::string zone_suffix;
};

enum class RenderSegmentKind : std::uint8_t {
    Literal,
    Timestamp,
    Privacy,
    SourceIp,
    DestinationIp
};

struct RenderSegment {
    std::string text;
    RenderSegmentKind kind{RenderSegmentKind::Literal};
    TimestampToken timestamp;
    PrivacyTokenKind privacy{PrivacyTokenKind::None};
};

struct CompiledLog {
    std::vector<RenderSegment> segments;
    std::size_t capacity_hint{0};
    bool has_timestamp{false};
    bool has_privacy{false};
    bool cache_privacy_profiles{false};
};

class PreparedLog {
public:
    PreparedLog(std::vector<RenderSegment> segments, std::chrono::seconds offset);
    PreparedLog(std::shared_ptr<const CompiledLog> compiled, std::string source_ip, std::string destination_ip, std::chrono::seconds offset);
    PreparedLog(const PreparedLog& other);
    PreparedLog& operator=(const PreparedLog& other);
    PreparedLog(PreparedLog&& other) noexcept = default;
    PreparedLog& operator=(PreparedLog&& other) noexcept = default;
    [[nodiscard]] std::string_view render(std::chrono::system_clock::time_point now, bool calendar_time = false);
    [[nodiscard]] std::size_t capacity_hint() const noexcept;

private:
    void initialize_cache();
    void initialize_random_state() noexcept;
    [[nodiscard]] std::size_t next_profile_index() noexcept;
    void append_non_timestamp_segment(std::string& output, const RenderSegment& segment, std::size_t profile_index) const;

    std::shared_ptr<const CompiledLog> compiled_;
    std::string source_ip_;
    std::string destination_ip_;
    std::string cached_;
    std::vector<std::string> timestamp_cache_;
    std::array<std::string, PrivacyAnonymizer::synthetic_profile_count> privacy_cache_;
    std::array<std::int64_t, PrivacyAnonymizer::synthetic_profile_count> privacy_cache_seconds_{};
    std::array<bool, PrivacyAnonymizer::synthetic_profile_count> privacy_cache_calendar_time_{};
    std::array<bool, PrivacyAnonymizer::synthetic_profile_count> privacy_cache_valid_{};
    std::int64_t cached_second_{-1};
    bool cached_calendar_time_{false};
    std::chrono::seconds offset_{0};
    std::uint64_t random_state_{0};
};

class LogRenderer {
public:
    [[nodiscard]] static domain::LogTemplate tokenize(domain::LogTemplate item);
    [[nodiscard]] static std::string tokenize(std::string_view sample);
    [[nodiscard]] static std::shared_ptr<const CompiledLog> compile(const domain::LogTemplate& item);
    [[nodiscard]] static PreparedLog bind(std::shared_ptr<const CompiledLog> compiled, std::string source_ip, std::string destination_ip, std::chrono::seconds offset);
    [[nodiscard]] static LogTemplateAnalysis analyze(const CompiledLog& compiled);
    [[nodiscard]] static LogTemplateAnalysis analyze(const domain::LogTemplate& item);
    [[nodiscard]] static LogTemplateAnalysis analyze(std::string_view sample);
    [[nodiscard]] static std::vector<PreparedLog> prepare(const domain::GeneratorConfig& config);
    [[nodiscard]] static PreparedLog prepare_one(const domain::LogTemplate& item, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
    [[nodiscard]] static PreparedLog prepare_one(std::string sample, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
};

}
