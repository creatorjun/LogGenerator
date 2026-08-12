// src/infrastructure/timestamp_identifier_generator.cpp
#include "infrastructure/timestamp_identifier_generator.hpp"

#include <chrono>
#include <format>

namespace loggen::infrastructure {

std::string TimestampIdentifierGenerator::next(const std::string_view prefix) {
    const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto serial = sequence_.fetch_add(1, std::memory_order_relaxed);
    return std::format("{}-{:x}-{:x}", prefix, static_cast<std::uint64_t>(ticks), serial);
}

}
