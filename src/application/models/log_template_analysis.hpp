// src/application/models/log_template_analysis.hpp
#pragma once

#include <cstddef>
#include <cstdint>
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

}
