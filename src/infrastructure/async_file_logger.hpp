// src/infrastructure/async_file_logger.hpp
#pragma once

#include "application/ports/logger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>

namespace loggen::infrastructure {

class AsyncFileLogger final : public application::ILogger {
public:
    explicit AsyncFileLogger(std::filesystem::path directory, std::string base_name = "LogGenerator", std::size_t queue_capacity = 8192);
    ~AsyncFileLogger() override;

    AsyncFileLogger(const AsyncFileLogger&) = delete;
    AsyncFileLogger& operator=(const AsyncFileLogger&) = delete;

    void log(application::LogLevel level, std::string_view message) noexcept override;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;
    [[nodiscard]] std::uint64_t dropped_entries() const noexcept;

private:
    struct Entry {
        std::chrono::system_clock::time_point timestamp;
        application::LogLevel level;
        std::uint32_t thread_id;
        std::string message;
    };

    void run(std::stop_token stop_token) noexcept;
    void open_for(std::chrono::system_clock::time_point timestamp);
    void write_entry(const Entry& entry);
    void write_dropped_notice(std::uint64_t count);
    [[nodiscard]] static std::string date_key(std::chrono::system_clock::time_point timestamp);
    [[nodiscard]] static std::string format_entry(const Entry& entry);

    std::filesystem::path directory_;
    std::string base_name_;
    std::size_t queue_capacity_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Entry> queue_;
    std::atomic<bool> accepting_{true};
    std::atomic<std::uint64_t> dropped_pending_{0};
    std::atomic<std::uint64_t> dropped_total_{0};
    std::ofstream output_;
    std::string active_date_;
    std::jthread worker_;
};

}
