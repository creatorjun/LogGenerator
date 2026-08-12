// src/application/log_template_analysis.hpp
#pragma once

#include <cstddef>
#include <cstdint>
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
    MonthDayYear,
    DateOnly,
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

}
