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
        return reserve(1);
    }

    [[nodiscard]] std::size_t reserve(const std::size_t count) noexcept {
        if (item_count_ == 1) {
            return 0;
        }
        return next_.fetch_add(count, std::memory_order_relaxed) % item_count_;
    }

    [[nodiscard]] std::size_t item_count() const noexcept {
        return item_count_;
    }

private:
    std::size_t item_count_{0};
    alignas(64) std::atomic<std::size_t> next_{0};
};

}
