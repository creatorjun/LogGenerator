// src/domain/log_level.hpp
#pragma once

#include <string_view>

namespace loggen::domain {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

constexpr std::string_view log_level_name(const LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Critical:
        return "CRITICAL";
    }
    return "UNKNOWN";
}

}
