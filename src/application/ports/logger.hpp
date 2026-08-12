// src/application/ports/logger.hpp
#pragma once

#include "domain/log_level.hpp"

#include <string_view>

namespace loggen::application {

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(domain::LogLevel level, std::string_view message) noexcept = 0;

    void debug(const std::string_view message) noexcept {
        log(domain::LogLevel::Debug, message);
    }

    void info(const std::string_view message) noexcept {
        log(domain::LogLevel::Info, message);
    }

    void warning(const std::string_view message) noexcept {
        log(domain::LogLevel::Warning, message);
    }

    void error(const std::string_view message) noexcept {
        log(domain::LogLevel::Error, message);
    }

    void critical(const std::string_view message) noexcept {
        log(domain::LogLevel::Critical, message);
    }
};

}
