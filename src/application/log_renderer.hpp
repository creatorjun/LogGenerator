// src/application/log_renderer.hpp
#pragma once

#include "application/privacy_anonymizer.hpp"
#include "domain/generator_config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

enum class TimestampStyle {
    Iso8601,
    YearFirst,
    SyslogWithYear,
    SyslogWithoutYear,
    MonthFirstGmt,
    Apache,
    Compact,
    CompactWithT,
    CompactDate,
    CompactYearMonth,
    CompactTime,
    YearFirstMinute,
    MonthDayYear,
    DateOnly,
    YearMonth,
    TimeOnly
};

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

struct RenderSegment {
    std::string text;
    bool is_timestamp{false};
    TimestampToken timestamp;
    PrivacyTokenKind privacy{PrivacyTokenKind::None};
};

struct LogTemplateAnalysis {
    std::size_t timestamp_count{0};
    std::size_t source_ip_count{0};
    std::size_t destination_ip_count{0};
    std::size_t privacy_token_count{0};
    std::uint32_t privacy_token_mask{0};
    std::vector<TimestampStyle> timestamp_styles;
};

[[nodiscard]] constexpr std::string_view timestamp_style_name(const TimestampStyle style) noexcept {
    switch (style) {
    case TimestampStyle::Iso8601:
        return "ISO 8601 / Year First";
    case TimestampStyle::YearFirst:
        return "Year First";
    case TimestampStyle::SyslogWithYear:
        return "Syslog With Year";
    case TimestampStyle::SyslogWithoutYear:
        return "Syslog";
    case TimestampStyle::MonthFirstGmt:
        return "Month First GMT";
    case TimestampStyle::Apache:
        return "Apache";
    case TimestampStyle::Compact:
        return "Compact yyyyMMddHHmmss";
    case TimestampStyle::CompactWithT:
        return "Compact yyyyMMddTHHmmss";
    case TimestampStyle::CompactDate:
        return "Compact yyyyMMdd";
    case TimestampStyle::CompactYearMonth:
        return "Compact yyyyMM";
    case TimestampStyle::CompactTime:
        return "Compact HHmmss";
    case TimestampStyle::YearFirstMinute:
        return "Year First yyyy-MM-dd HH:mm";
    case TimestampStyle::MonthDayYear:
        return "MMM dd yyyy HH:mm:ss";
    case TimestampStyle::DateOnly:
        return "Separated Date";
    case TimestampStyle::YearMonth:
        return "Year Month";
    case TimestampStyle::TimeOnly:
        return "Separated Time";
    }
    return "Unknown";
}

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
        bool has_privacy{false};
    };

    void initialize_cache();
    void initialize_random_state() noexcept;
    [[nodiscard]] std::size_t next_profile_index() noexcept;

    std::shared_ptr<const CompiledLog> compiled_;
    std::string cached_;
    std::vector<std::string> timestamp_cache_;
    std::int64_t cached_second_{-1};
    bool cached_calendar_time_{false};
    std::chrono::seconds offset_{0};
    std::uint64_t random_state_{0};
};

class LogRenderer {
public:
    [[nodiscard]] static LogTemplateAnalysis analyze(const domain::LogTemplate& item);
    [[nodiscard]] static LogTemplateAnalysis analyze(std::string_view sample);
    [[nodiscard]] static std::vector<PreparedLog> prepare(const domain::GeneratorConfig& config);
    [[nodiscard]] static PreparedLog prepare_one(const domain::LogTemplate& item, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
    [[nodiscard]] static PreparedLog prepare_one(std::string sample, const std::string& source_ip, const std::string& destination_ip, std::chrono::seconds offset);
};

}
