// src/domain/sample_id.hpp
#pragma once

#include <algorithm>
#include <string_view>

namespace loggen::domain {

[[nodiscard]] inline bool valid_sample_id(const std::string_view value) noexcept {
    return !value.empty() && std::ranges::all_of(value, [](const char character) {
        return character >= '0' && character <= '9';
    });
}

}
