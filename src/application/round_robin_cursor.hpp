// src/application/round_robin_cursor.hpp
#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>

namespace loggen::application {

class RoundRobinCursor final {
public:
    explicit RoundRobinCursor(const std::size_t item_count)
        : item_count_(item_count) {
        if (item_count_ == 0) {
            throw std::invalid_argument("Round-robin item count must be greater than zero");
        }
    }

    [[nodiscard]] std::size_t next() noexcept {
        auto current = next_.load(std::memory_order_relaxed);
        while (true) {
            const auto following = current + 1 == item_count_ ? 0 : current + 1;
            if (next_.compare_exchange_weak(current, following, std::memory_order_relaxed, std::memory_order_relaxed)) {
                return current;
            }
        }
    }

private:
    std::size_t item_count_{0};
    std::atomic<std::size_t> next_{0};
};

}
