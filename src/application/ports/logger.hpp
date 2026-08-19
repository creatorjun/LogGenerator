// src/application/ports/logger.hpp
#pragma once

#include <string_view>

namespace loggen::application {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

[[nodiscard]] constexpr std::string_view log_level_name(const LogLevel level) noexcept {
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

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(LogLevel level, std::string_view message) noexcept = 0;

    void debug(const std::string_view message) noexcept {
        log(LogLevel::Debug, message);
    }

    void info(const std::string_view message) noexcept {
        log(LogLevel::Info, message);
    }

    void warning(const std::string_view message) noexcept {
        log(LogLevel::Warning, message);
    }

    void error(const std::string_view message) noexcept {
        log(LogLevel::Error, message);
    }

    void critical(const std::string_view message) noexcept {
        log(LogLevel::Critical, message);
    }
};

}
