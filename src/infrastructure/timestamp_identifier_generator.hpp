// src/infrastructure/timestamp_identifier_generator.hpp
#pragma once

#include "application/ports/identifier_generator.hpp"

#include <atomic>
#include <cstdint>

namespace loggen::infrastructure {

class TimestampIdentifierGenerator final : public application::IIdentifierGenerator {
public:
    [[nodiscard]] std::string next(std::string_view prefix) override;

private:
    std::atomic<std::uint64_t> sequence_{0};
};

}
