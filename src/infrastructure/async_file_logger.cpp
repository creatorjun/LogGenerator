// src/infrastructure/async_file_logger.cpp
#include "infrastructure/async_file_logger.hpp"

#include <array>
#include <cstdio>
#include <ctime>
#include <format>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace loggen::infrastructure {
namespace {

std::tm local_time(const std::chrono::system_clock::time_point timestamp) {
    const auto raw_time = std::chrono::system_clock::to_time_t(timestamp);
    static std::mutex conversion_mutex;
    const std::scoped_lock lock(conversion_mutex);
    std::tm result{};
    if (const auto* converted = std::localtime(&raw_time); converted != nullptr) {
        result = *converted;
    }
    return result;
}

std::uint32_t current_thread_id() noexcept {
    return static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

std::string sanitize(const std::string_view message) {
    std::string result;
    result.reserve(message.size());
    for (const char character : message) {
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            result.append("\\n");
            continue;
        }
        result.push_back(character);
    }
    return result;
}

}

AsyncFileLogger::AsyncFileLogger(std::filesystem::path directory, std::string base_name, const std::size_t queue_capacity)
    : directory_(std::move(directory)), base_name_(std::move(base_name)), queue_capacity_(queue_capacity) {
    if (base_name_.empty()) {
        throw std::invalid_argument("Logger base name must not be empty");
    }
    if (queue_capacity_ == 0) {
        throw std::invalid_argument("Logger queue capacity must be greater than zero");
    }
    std::filesystem::create_directories(directory_);
    open_for(std::chrono::system_clock::now());
    worker_ = std::jthread([this](const std::stop_token stop_token) {
        run(stop_token);
    });
}

AsyncFileLogger::~AsyncFileLogger() {
    accepting_.store(false, std::memory_order_release);
    worker_.request_stop();
    condition_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    output_.flush();
    output_.close();
}

void AsyncFileLogger::log(const application::LogLevel level, const std::string_view message) noexcept {
    if (!accepting_.load(std::memory_order_acquire)) {
        return;
    }
    try {
        Entry entry{std::chrono::system_clock::now(), level, current_thread_id(), std::string(message)};
        {
            std::scoped_lock lock(mutex_);
            if (!accepting_.load(std::memory_order_relaxed)) {
                return;
            }
            if (queue_.size() >= queue_capacity_) {
                dropped_pending_.fetch_add(1, std::memory_order_relaxed);
                dropped_total_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            queue_.push_back(std::move(entry));
        }
        condition_.notify_one();
    } catch (...) {
        dropped_pending_.fetch_add(1, std::memory_order_relaxed);
        dropped_total_.fetch_add(1, std::memory_order_relaxed);
    }
}

const std::filesystem::path& AsyncFileLogger::directory() const noexcept {
    return directory_;
}

std::uint64_t AsyncFileLogger::dropped_entries() const noexcept {
    return dropped_total_.load(std::memory_order_relaxed);
}

void AsyncFileLogger::run(const std::stop_token stop_token) noexcept {
    std::deque<Entry> pending;
    try {
        auto last_flush = std::chrono::steady_clock::now();
        while (true) {
            {
                std::unique_lock lock(mutex_);
                condition_.wait_for(lock, std::chrono::milliseconds{500}, [this, stop_token] {
                    return stop_token.stop_requested() || !queue_.empty();
                });
                queue_.swap(pending);
                if (pending.empty() && stop_token.stop_requested()) {
                    break;
                }
            }

            bool urgent = false;
            for (const auto& entry : pending) {
                open_for(entry.timestamp);
                write_entry(entry);
                urgent = urgent || entry.level == application::LogLevel::Error || entry.level == application::LogLevel::Critical;
            }
            pending.clear();
            const auto dropped = dropped_pending_.exchange(0, std::memory_order_relaxed);
            if (dropped > 0) {
                write_dropped_notice(dropped);
                urgent = true;
            }
            const auto now = std::chrono::steady_clock::now();
            if (urgent || stop_token.stop_requested() || now - last_flush >= std::chrono::seconds{1}) {
                output_.flush();
                last_flush = now;
            }
        }

        const auto dropped = dropped_pending_.exchange(0, std::memory_order_relaxed);
        if (dropped > 0) {
            write_dropped_notice(dropped);
        }
        output_.flush();
    } catch (const std::exception& error) {
        const auto message = std::string("LogGenerator file logger failure: ") + error.what() + "\n";
        std::fputs(message.c_str(), stderr);
    } catch (...) {
        std::fputs("LogGenerator file logger failure: unknown error\n", stderr);
    }
}

void AsyncFileLogger::open_for(const std::chrono::system_clock::time_point timestamp) {
    const auto requested_date = date_key(timestamp);
    if (output_.is_open() && requested_date == active_date_) {
        return;
    }
    if (output_.is_open()) {
        output_.flush();
        output_.close();
    }
    const auto path = directory_ / (base_name_ + '_' + requested_date + ".log");
    output_.open(path, std::ios::binary | std::ios::app);
    if (!output_) {
        throw std::runtime_error("Unable to open log file: " + path.string());
    }
    active_date_ = requested_date;
}

void AsyncFileLogger::write_entry(const Entry& entry) {
    const auto line = format_entry(entry);
    output_.write(line.data(), static_cast<std::streamsize>(line.size()));
    if (!output_) {
        throw std::runtime_error("Unable to write application log");
    }
}

void AsyncFileLogger::write_dropped_notice(const std::uint64_t count) {
    const Entry entry{std::chrono::system_clock::now(), application::LogLevel::Warning, current_thread_id(), std::format("Logger queue discarded {} entries", count)};
    open_for(entry.timestamp);
    write_entry(entry);
}

std::string AsyncFileLogger::date_key(const std::chrono::system_clock::time_point timestamp) {
    const auto value = local_time(timestamp);
    std::array<char, 16> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%04d%02d%02d", value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    return buffer.data();
}

std::string AsyncFileLogger::format_entry(const Entry& entry) {
    const auto value = local_time(entry.timestamp);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()).count() % 1000;
    std::array<char, 64> timestamp{};
    std::snprintf(timestamp.data(), timestamp.size(), "%04d-%02d-%02d %02d:%02d:%02d.%03lld", value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec, static_cast<long long>(milliseconds));
    return std::format("[{}] [{}] [T{}] {}\n", timestamp.data(), application::log_level_name(entry.level), entry.thread_id, sanitize(entry.message));
}

}
