// src/application/log_renderer.cpp
#include "application/log_renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <regex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace loggen::application {
namespace {

struct MatchCandidate {
    std::size_t position{0};
    std::size_t length{0};
    int priority{0};
    TimestampToken token;
};

struct TimestampPattern {
    std::regex expression;
    TimestampStyle style;
    int priority;
    std::size_t capture_group{0};
};

const std::regex& source_field_pattern() {
    static const std::regex pattern(
        R"(((?:^|[\s,;|{])["']?(?:src|srcip|src_ip|src-ip|srp_ip|srcaddr|src_addr|srcaddress|src_address|sourceip|source_ip|sourceaddress|source_address|source-address|clientip|client_ip|clientipaddr|sip)["']?\s*[:=]\s*["']?)(\d{1,3}(?:\.\d{1,3}){3}))",
        std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
    return pattern;
}

const std::regex& destination_field_pattern() {
    static const std::regex pattern(
        R"(((?:^|[\s,;|{])["']?(?:dst|dstip|dst_ip|dest_ip|dstnip|dstn_ip|dstaddr|dst_addr|dstaddress|dst_address|destinationip|destination_ip|destinationaddress|destination_address|destination-address|serverip|server_ip|dip)["']?\s*[:=]\s*["']?)(\d{1,3}(?:\.\d{1,3}){3}))",
        std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
    return pattern;
}

const std::regex& arrow_source_pattern() {
    static const std::regex pattern(
        R"((\b)(\d{1,3}(?:\.\d{1,3}){3})(?=:\d{1,5}\s*(?:-|=)>))",
        std::regex::ECMAScript | std::regex::optimize);
    return pattern;
}

const std::regex& arrow_destination_pattern() {
    static const std::regex pattern(
        R"(((?:-|=)>\s*)(\d{1,3}(?:\.\d{1,3}){3}))",
        std::regex::ECMAScript | std::regex::optimize);
    return pattern;
}

const std::vector<TimestampPattern>& timestamp_patterns() {
    static const std::vector<TimestampPattern> patterns{
        {std::regex(R"(\b\d{4}[-/]\d{2}[-/]\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?(?:Z|[+-]\d{2}:?\d{2})?)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Iso8601, 0, 0},
        {std::regex(R"(\b(?:[A-Z][a-z]{2}\s+)?[A-Z][a-z]{2}\s+\d{1,2}\s+\d{4}\s+\d{2}:\d{2}:\d{2}(?:\s+(?:GMT|UTC))?\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::MonthDayYear, 1, 0},
        {std::regex(R"(\b(?:[A-Z][a-z]{2}\s+)?[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\s+\d{4}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::SyslogWithYear, 2, 0},
        {std::regex(R"(\b\d{1,2}/[A-Z][a-z]{2}/\d{4}:\d{2}:\d{2}:\d{2}\s+[+-]\d{4}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Apache, 3, 0},
        {std::regex(R"(\b(?:[A-Z][a-z]{2}\s+)?[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::SyslogWithoutYear, 4, 0},
        {std::regex(R"(\b\d{14}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Compact, 5, 0},
        {std::regex(R"((\bdate\s*=\s*["']?)(\d{4}[-/]\d{2}[-/]\d{2}))", std::regex::ECMAScript | std::regex::icase | std::regex::optimize), TimestampStyle::DateOnly, 6, 2},
        {std::regex(R"((\btime\s*=\s*["']?)(\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?))", std::regex::ECMAScript | std::regex::icase | std::regex::optimize), TimestampStyle::TimeOnly, 7, 2},
    };
    return patterns;
}

std::string replace_capture(const std::string& input, const std::regex& pattern, const std::string& replacement) {
    std::string output;
    output.reserve(input.size() + replacement.size());
    std::size_t cursor = 0;
    for (std::sregex_iterator iterator(input.begin(), input.end(), pattern), end; iterator != end; ++iterator) {
        const auto& match = *iterator;
        if (match.size() < 3 || !match[2].matched) {
            continue;
        }
        const auto begin = static_cast<std::size_t>(match[2].first - input.begin());
        const auto finish = static_cast<std::size_t>(match[2].second - input.begin());
        if (begin < cursor) {
            continue;
        }
        output.append(input, cursor, begin - cursor);
        output.append(replacement);
        cursor = finish;
    }
    output.append(input, cursor, std::string::npos);
    return output;
}

int parse_zone_offset(const std::string_view suffix) {
    if (suffix.size() < 5 || (suffix.front() != '+' && suffix.front() != '-')) {
        return 0;
    }
    const bool negative = suffix.front() == '-';
    const auto hours = (suffix[1] - '0') * 10 + (suffix[2] - '0');
    const std::size_t minute_index = suffix.size() == 6 ? 4 : 3;
    const auto minutes = (suffix[minute_index] - '0') * 10 + (suffix[minute_index + 1] - '0');
    const int total = hours * 60 + minutes;
    return negative ? -total : total;
}

TimestampToken make_token(const TimestampStyle style, const std::string_view value) {
    TimestampToken token;
    token.style = style;
    static constexpr std::array<std::string_view, 7> weekdays{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (value.size() > 4 && value[3] == ' ' && std::ranges::find(weekdays, value.substr(0, 3)) != weekdays.end()) {
        token.has_weekday = true;
    }
    if (style == TimestampStyle::Iso8601) {
        token.date_separator = value.size() > 4 ? value[4] : '-';
        token.date_time_separator = value.size() > 10 ? value[10] : 'T';
        const auto dot = value.find('.', 19);
        if (dot != std::string_view::npos) {
            auto end = dot + 1;
            while (end < value.size() && value[end] >= '0' && value[end] <= '9') {
                ++end;
            }
            token.fractional_digits = static_cast<std::uint8_t>(std::min<std::size_t>(9, end - dot - 1));
        }
        const auto zone_position = value.find_first_of("Z+-", 19);
        if (zone_position != std::string_view::npos) {
            token.zone_suffix = std::string(value.substr(zone_position));
            token.zone_offset_minutes = parse_zone_offset(token.zone_suffix);
        }
    } else if (style == TimestampStyle::Apache) {
        const auto zone_position = value.find_last_of("+-");
        if (zone_position != std::string_view::npos) {
            token.zone_suffix = std::string(value.substr(zone_position));
            token.zone_offset_minutes = parse_zone_offset(token.zone_suffix);
        }
    } else if (style == TimestampStyle::MonthFirstGmt) {
        token.zone_suffix = "GMT";
    } else if (style == TimestampStyle::MonthDayYear) {
        const auto zone_position = value.find_last_of(' ');
        if (zone_position != std::string_view::npos) {
            const auto suffix = value.substr(zone_position + 1);
            if (suffix == "GMT" || suffix == "UTC") {
                token.zone_suffix = std::string(suffix);
            }
        }
    } else if (style == TimestampStyle::DateOnly) {
        token.date_separator = value.size() > 4 ? value[4] : '-';
    } else if (style == TimestampStyle::TimeOnly) {
        const auto dot = value.find('.');
        if (dot != std::string_view::npos) {
            token.fractional_digits = static_cast<std::uint8_t>(std::min<std::size_t>(9, value.size() - dot - 1));
        }
    }
    return token;
}

std::vector<RenderSegment> compile_segments(const std::string& input) {
    std::vector<MatchCandidate> candidates;
    for (const auto& pattern : timestamp_patterns()) {
        for (std::sregex_iterator iterator(input.begin(), input.end(), pattern.expression), end; iterator != end; ++iterator) {
            const auto& match = *iterator;
            if (pattern.capture_group >= match.size() || !match[pattern.capture_group].matched) {
                continue;
            }
            const auto position = static_cast<std::size_t>(match[pattern.capture_group].first - input.begin());
            const auto length = static_cast<std::size_t>(match[pattern.capture_group].length());
            candidates.push_back(MatchCandidate{position, length, pattern.priority, make_token(pattern.style, std::string_view(input).substr(position, length))});
        }
    }
    std::ranges::sort(candidates, [](const MatchCandidate& left, const MatchCandidate& right) {
        if (left.position != right.position) {
            return left.position < right.position;
        }
        if (left.length != right.length) {
            return left.length > right.length;
        }
        return left.priority < right.priority;
    });

    std::vector<RenderSegment> segments;
    std::size_t cursor = 0;
    for (const auto& candidate : candidates) {
        if (candidate.position < cursor) {
            continue;
        }
        if (candidate.position > cursor) {
            segments.push_back(RenderSegment{input.substr(cursor, candidate.position - cursor), false, {}});
        }
        segments.push_back(RenderSegment{{}, true, candidate.token});
        cursor = candidate.position + candidate.length;
    }
    if (cursor < input.size()) {
        segments.push_back(RenderSegment{input.substr(cursor), false, {}});
    }
    if (segments.empty()) {
        segments.push_back(RenderSegment{input, false, {}});
    }
    return segments;
}

std::size_t count_captures(const std::string& input, const std::regex& pattern) {
    std::size_t count = 0;
    for (std::sregex_iterator iterator(input.begin(), input.end(), pattern), end; iterator != end; ++iterator) {
        if (iterator->size() >= 3 && (*iterator)[2].matched) {
            ++count;
        }
    }
    return count;
}

std::tm make_time(std::chrono::system_clock::time_point point, const TimestampToken& token, const bool calendar_time) {
    const auto value = std::chrono::system_clock::to_time_t(point);
    std::tm result{};
    if (calendar_time) {
        gmtime_s(&result, &value);
        return result;
    }
    const bool utc = token.zone_suffix == "Z" || token.zone_suffix == "GMT" || !token.zone_suffix.empty();
    if (utc && token.zone_offset_minutes != 0) {
        point += std::chrono::minutes{token.zone_offset_minutes};
    }
    const auto adjusted_value = std::chrono::system_clock::to_time_t(point);
    if (utc) {
        gmtime_s(&result, &adjusted_value);
    } else {
        localtime_s(&result, &adjusted_value);
    }
    return result;
}

void append_fraction(std::string& output, const std::chrono::system_clock::time_point point, const std::uint8_t digits) {
    if (digits == 0) {
        return;
    }
    auto remainder = point.time_since_epoch() % std::chrono::seconds{1};
    if (remainder.count() < 0) {
        remainder += std::chrono::seconds{1};
    }
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(remainder).count();
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), ".%09lld", static_cast<long long>(nanoseconds));
    output.append(buffer, static_cast<std::size_t>(digits) + 1);
}

void append_timestamp(std::string& output, const TimestampToken& token, const std::chrono::system_clock::time_point point, const bool calendar_time) {
    const auto value = make_time(point, token, calendar_time);
    static constexpr std::array<std::string_view, 12> months{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static constexpr std::array<std::string_view, 7> weekdays{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char buffer[64]{};
    if (token.has_weekday) {
        output.append(weekdays[static_cast<std::size_t>(value.tm_wday)]);
        output.push_back(' ');
    }
    switch (token.style) {
    case TimestampStyle::Iso8601:
    case TimestampStyle::YearFirst:
        std::snprintf(buffer, sizeof(buffer), "%04d%c%02d%c%02d%c%02d:%02d:%02d", value.tm_year + 1900, token.date_separator, value.tm_mon + 1, token.date_separator, value.tm_mday, token.date_time_separator, value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        append_fraction(output, point, token.fractional_digits);
        output.append(token.zone_suffix);
        break;
    case TimestampStyle::SyslogWithYear:
        std::snprintf(buffer, sizeof(buffer), "%s %02d %02d:%02d:%02d %04d", months[static_cast<std::size_t>(value.tm_mon)].data(), value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec, value.tm_year + 1900);
        output.append(buffer);
        break;
    case TimestampStyle::SyslogWithoutYear:
        std::snprintf(buffer, sizeof(buffer), "%s %02d %02d:%02d:%02d", months[static_cast<std::size_t>(value.tm_mon)].data(), value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        break;
    case TimestampStyle::MonthFirstGmt:
        std::snprintf(buffer, sizeof(buffer), "%s %02d %04d %02d:%02d:%02d GMT", months[static_cast<std::size_t>(value.tm_mon)].data(), value.tm_mday, value.tm_year + 1900, value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        break;
    case TimestampStyle::Apache:
        std::snprintf(buffer, sizeof(buffer), "%02d/%s/%04d:%02d:%02d:%02d %s", value.tm_mday, months[static_cast<std::size_t>(value.tm_mon)].data(), value.tm_year + 1900, value.tm_hour, value.tm_min, value.tm_sec, token.zone_suffix.c_str());
        output.append(buffer);
        break;
    case TimestampStyle::Compact:
        std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d%02d", value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        break;
    case TimestampStyle::MonthDayYear:
        std::snprintf(buffer, sizeof(buffer), "%s %02d %04d %02d:%02d:%02d", months[static_cast<std::size_t>(value.tm_mon)].data(), value.tm_mday, value.tm_year + 1900, value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        if (!token.zone_suffix.empty()) {
            output.push_back(' ');
            output.append(token.zone_suffix);
        }
        break;
    case TimestampStyle::DateOnly:
        std::snprintf(buffer, sizeof(buffer), "%04d%c%02d%c%02d", value.tm_year + 1900, token.date_separator, value.tm_mon + 1, token.date_separator, value.tm_mday);
        output.append(buffer);
        break;
    case TimestampStyle::TimeOnly:
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", value.tm_hour, value.tm_min, value.tm_sec);
        output.append(buffer);
        append_fraction(output, point, token.fractional_digits);
        break;
    }
}

}

PreparedLog::PreparedLog(std::vector<RenderSegment> segments, const std::chrono::seconds offset)
    : offset_(offset) {
    auto compiled = std::make_shared<CompiledLog>();
    compiled->segments = std::move(segments);
    for (const auto& segment : compiled->segments) {
        compiled->capacity_hint += segment.is_timestamp ? 40 : segment.text.size();
        compiled->has_timestamp = compiled->has_timestamp || segment.is_timestamp;
    }
    compiled_ = std::move(compiled);
    initialize_cache();
}

PreparedLog::PreparedLog(const PreparedLog& other)
    : compiled_(other.compiled_), offset_(other.offset_) {
    initialize_cache();
}

PreparedLog& PreparedLog::operator=(const PreparedLog& other) {
    if (this != &other) {
        compiled_ = other.compiled_;
        offset_ = other.offset_;
        cached_.clear();
        cached_second_ = -1;
        initialize_cache();
    }
    return *this;
}

void PreparedLog::initialize_cache() {
    if (!compiled_) {
        return;
    }
    cached_.reserve(compiled_->capacity_hint);
    if (!compiled_->has_timestamp) {
        for (const auto& segment : compiled_->segments) {
            cached_.append(segment.text);
        }
    }
}

std::string_view PreparedLog::render(const std::chrono::system_clock::time_point now, const bool calendar_time) {
    if (!compiled_->has_timestamp) {
        return cached_;
    }
    const auto adjusted = now + offset_;
    const auto second = std::chrono::duration_cast<std::chrono::seconds>(adjusted.time_since_epoch()).count();
    if (second == cached_second_ && calendar_time == cached_calendar_time_) {
        return cached_;
    }
    cached_.clear();
    for (const auto& segment : compiled_->segments) {
        if (segment.is_timestamp) {
            append_timestamp(cached_, segment.timestamp, adjusted, calendar_time);
        } else {
            cached_.append(segment.text);
        }
    }
    cached_second_ = second;
    cached_calendar_time_ = calendar_time;
    return cached_;
}

std::size_t PreparedLog::capacity_hint() const noexcept {
    return compiled_ == nullptr ? 0 : compiled_->capacity_hint;
}

LogTemplateAnalysis LogRenderer::analyze(const std::string_view sample) {
    const std::string input{sample};
    LogTemplateAnalysis result;
    const auto segments = compile_segments(input);
    for (const auto& segment : segments) {
        if (!segment.is_timestamp) {
            continue;
        }
        ++result.timestamp_count;
        if (std::ranges::find(result.timestamp_styles, segment.timestamp.style) == result.timestamp_styles.end()) {
            result.timestamp_styles.push_back(segment.timestamp.style);
        }
    }
    result.source_ip_count = count_captures(input, source_field_pattern()) + count_captures(input, arrow_source_pattern());
    result.destination_ip_count = count_captures(input, destination_field_pattern()) + count_captures(input, arrow_destination_pattern());
    return result;
}

std::vector<PreparedLog> LogRenderer::prepare(const domain::GeneratorConfig& config) {
    std::vector<PreparedLog> result;
    result.reserve(config.templates.size());
    for (const auto& item : config.templates) {
        const auto offset = config.timestamp_generation.mode == domain::TimestampGenerationMode::Offset ? config.timestamp_generation.offset.value() : std::chrono::seconds{0};
        result.push_back(prepare_one(item.sample, config.source_ip, config.destination_ip, offset));
    }
    return result;
}

PreparedLog LogRenderer::prepare_one(std::string sample, const std::string& source_ip, const std::string& destination_ip, const std::chrono::seconds offset) {
    sample = replace_capture(sample, source_field_pattern(), source_ip);
    sample = replace_capture(sample, destination_field_pattern(), destination_ip);
    sample = replace_capture(sample, arrow_source_pattern(), source_ip);
    sample = replace_capture(sample, arrow_destination_pattern(), destination_ip);
    return PreparedLog{compile_segments(sample), offset};
}

}
