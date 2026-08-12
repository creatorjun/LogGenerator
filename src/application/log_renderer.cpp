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
};

const std::regex& source_field_pattern() {
    static const std::regex pattern(
        R"(((?:^|[\s,;|{])["']?(?:src|srcip|src_ip|srcaddr|src_addr|srcaddress|src_address|sourceip|source_ip|sourceaddress|source_address|clientip|client_ip|sip)["']?\s*[:=]\s*["']?)(\d{1,3}(?:\.\d{1,3}){3}))",
        std::regex::ECMAScript | std::regex::icase | std::regex::optimize);
    return pattern;
}

const std::regex& destination_field_pattern() {
    static const std::regex pattern(
        R"(((?:^|[\s,;|{])["']?(?:dst|dstip|dst_ip|dstnip|dstn_ip|dstaddr|dst_addr|dstaddress|dst_address|destinationip|destination_ip|destinationaddress|destination_address|serverip|server_ip|dip)["']?\s*[:=]\s*["']?)(\d{1,3}(?:\.\d{1,3}){3}))",
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
        {std::regex(R"(\b\d{4}[-/]\d{2}[-/]\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?(?:Z|[+-]\d{2}:?\d{2})?)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Iso8601, 0},
        {std::regex(R"(\b[A-Z][a-z]{2}\s+\d{1,2}\s+\d{4}\s+\d{2}:\d{2}:\d{2}\s+GMT\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::MonthFirstGmt, 1},
        {std::regex(R"(\b[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\s+\d{4}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::SyslogWithYear, 2},
        {std::regex(R"(\b\d{1,2}/[A-Z][a-z]{2}/\d{4}:\d{2}:\d{2}:\d{2}\s+[+-]\d{4}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Apache, 3},
        {std::regex(R"(\b[A-Z][a-z]{2}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::SyslogWithoutYear, 4},
        {std::regex(R"(\b\d{14}\b)", std::regex::ECMAScript | std::regex::optimize), TimestampStyle::Compact, 5},
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
    }
    return token;
}

std::vector<RenderSegment> compile_segments(const std::string& input) {
    std::vector<MatchCandidate> candidates;
    for (const auto& pattern : timestamp_patterns()) {
        for (std::sregex_iterator iterator(input.begin(), input.end(), pattern.expression), end; iterator != end; ++iterator) {
            const auto& match = *iterator;
            const auto position = static_cast<std::size_t>(match.position());
            const auto length = static_cast<std::size_t>(match.length());
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

std::tm make_time(std::chrono::system_clock::time_point point, const TimestampToken& token) {
    const bool utc = token.zone_suffix == "Z" || token.zone_suffix == "GMT" || !token.zone_suffix.empty();
    if (utc && token.zone_offset_minutes != 0) {
        point += std::chrono::minutes{token.zone_offset_minutes};
    }
    const auto value = std::chrono::system_clock::to_time_t(point);
    std::tm result{};
    if (utc) {
        gmtime_s(&result, &value);
    } else {
        localtime_s(&result, &value);
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

void append_timestamp(std::string& output, const TimestampToken& token, const std::chrono::system_clock::time_point point) {
    const auto value = make_time(point, token);
    static constexpr std::array<std::string_view, 12> months{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buffer[64]{};
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
    }
}

}

PreparedLog::PreparedLog(std::vector<RenderSegment> segments, const std::chrono::seconds offset)
    : segments_(std::move(segments)), offset_(offset) {
    for (const auto& segment : segments_) {
        capacity_hint_ += segment.is_timestamp ? 40 : segment.text.size();
    }
    cached_.reserve(capacity_hint_);
}

std::string_view PreparedLog::render(const std::chrono::system_clock::time_point now) {
    const auto adjusted = now + offset_;
    const auto second = std::chrono::duration_cast<std::chrono::seconds>(adjusted.time_since_epoch()).count();
    if (second == cached_second_) {
        return cached_;
    }
    cached_.clear();
    for (const auto& segment : segments_) {
        if (segment.is_timestamp) {
            append_timestamp(cached_, segment.timestamp, adjusted);
        } else {
            cached_.append(segment.text);
        }
    }
    cached_second_ = second;
    return cached_;
}

std::size_t PreparedLog::capacity_hint() const noexcept {
    return capacity_hint_;
}

std::vector<PreparedLog> LogRenderer::prepare(const domain::GeneratorConfig& config) {
    std::vector<PreparedLog> result;
    result.reserve(config.templates.size());
    for (const auto& item : config.templates) {
        result.push_back(prepare_one(item.sample, config.source_ip, config.destination_ip, config.time_offset.value()));
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
