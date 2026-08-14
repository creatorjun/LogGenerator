// src/infrastructure/file_transport.cpp
#include "infrastructure/file_transport.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <format>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace loggen::infrastructure {
namespace {

class FileCollision final : public std::exception {
};

std::tm local_time(const std::time_t value) {
    static std::mutex conversion_mutex;
    const std::scoped_lock lock(conversion_mutex);
    const auto* converted = std::localtime(&value);
    return converted == nullptr ? std::tm{} : *converted;
}

class FileHandle final {
public:
#ifdef _WIN32
    using Value = HANDLE;
    static inline const Value invalid = INVALID_HANDLE_VALUE;
#else
    using Value = int;
    static inline constexpr Value invalid = -1;
#endif

    explicit FileHandle(const Value value) noexcept
        : value_(value) {
    }

    ~FileHandle() {
        static_cast<void>(close());
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    void write_all(const std::string_view content) {
        std::size_t offset = 0;
        while (offset < content.size()) {
            const auto remaining = content.size() - offset;
#ifdef _WIN32
            const auto chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
            DWORD written = 0;
            if (!WriteFile(value_, content.data() + offset, chunk, &written, nullptr)) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log write failed");
            }
#else
            const auto chunk = std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
            const ssize_t written = ::write(value_, content.data() + offset, chunk);
            if (written < 0) {
                throw std::system_error(errno, std::system_category(), "Generated log write failed");
            }
#endif
            if (written == 0) {
                throw std::runtime_error("Generated log write made no progress");
            }
            offset += static_cast<std::size_t>(written);
        }
    }

    [[nodiscard]] bool close() noexcept {
        if (value_ == invalid) {
            return true;
        }
        const auto value = value_;
        value_ = invalid;
#ifdef _WIN32
        return CloseHandle(value) != FALSE;
#else
        return ::close(value) == 0;
#endif
    }

private:
    Value value_{invalid};
};

FileHandle create_exclusive(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto value = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (value == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
            throw FileCollision{};
        }
        throw std::system_error(static_cast<int>(error), std::system_category(), "Generated log file creation failed");
    }
#else
    const int value = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (value < 0) {
        const int error = errno;
        if (error == EEXIST) {
            throw FileCollision{};
        }
        throw std::system_error(error, std::system_category(), "Generated log file creation failed");
    }
#endif
    return FileHandle{value};
}

int close_error() noexcept {
#ifdef _WIN32
    return static_cast<int>(GetLastError());
#else
    return errno;
#endif
}

}

FileTransport::FileTransport(std::filesystem::path output_directory)
    : output_directory_(std::move(output_directory)) {
}

FileTransport::~FileTransport() = default;

void FileTransport::connect(const domain::EndpointConfig& endpoint) {
    timestamp_.clear();
    if (!endpoint.file_output_directory.empty()) {
        output_directory_ = endpoint.file_output_directory;
    }
    std::filesystem::create_directories(output_directory_);
    const auto now = std::chrono::system_clock::now();
    const auto raw_time = std::chrono::system_clock::to_time_t(now);
    const auto value = local_time(raw_time);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    if (milliseconds < 0) {
        milliseconds += 1000;
    }
    timestamp_ = std::format("{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}", value.tm_year + 1900, value.tm_mon + 1, value.tm_mday, value.tm_hour, value.tm_min, value.tm_sec, milliseconds);
    next_file_index_ = 0;
    file_count_ = 0;
    total_size_ = 0;
    max_total_bytes_ = endpoint.file_max_total_bytes;
    max_file_count_ = endpoint.file_max_count;
    max_duration_ = endpoint.file_max_duration;
    started_at_ = std::chrono::steady_clock::now();
}

application::SendResult FileTransport::send(const std::string_view payload) {
    if (timestamp_.empty()) {
        throw std::runtime_error("FILE transport is not connected");
    }
    if (payload.empty()) {
        return application::SendResult::Sent;
    }
    if (max_duration_.count() > 0 && std::chrono::steady_clock::now() - started_at_ >= max_duration_) {
        return application::SendResult::DurationLimitReached;
    }
    if (max_total_bytes_ > 0 && (total_size_ >= max_total_bytes_ || payload.size() > max_total_bytes_ - total_size_)) {
        return application::SendResult::TotalBytesLimitReached;
    }

    const auto newline_count = static_cast<std::uint64_t>(std::ranges::count(payload, '\n'));
    const auto log_count = newline_count + (payload.back() == '\n' ? 0ULL : 1ULL);
    if (max_file_count_ > 0 && (file_count_ >= max_file_count_ || log_count > max_file_count_ - file_count_)) {
        return application::SendResult::FileCountLimitReached;
    }

    std::size_t log_start = 0;
    while (log_start < payload.size()) {
        const auto newline = payload.find('\n', log_start);
        const auto log = newline == std::string_view::npos ? payload.substr(log_start) : payload.substr(log_start, newline - log_start + 1);
        for (;;) {
            const auto path = file_path(next_file_index_);
            ++next_file_index_;
            try {
                write_log(path, log);
                break;
            } catch (const FileCollision&) {
            }
        }
        ++file_count_;
        total_size_ += log.size();
        if (newline == std::string_view::npos) {
            break;
        }
        log_start = newline + 1;
    }
    return application::SendResult::Sent;
}

bool FileTransport::is_datagram() const noexcept {
    return false;
}

void FileTransport::write_log(const std::filesystem::path& path, const std::string_view log) {
    auto file = create_exclusive(path);
    try {
        file.write_all(log);
        if (!file.close()) {
            throw std::system_error(close_error(), std::system_category(), "Generated log close failed");
        }
    } catch (...) {
        static_cast<void>(file.close());
        std::error_code cleanup_error;
        std::filesystem::remove(path, cleanup_error);
        throw;
    }
}

std::filesystem::path FileTransport::file_path(const std::uint64_t index) const {
    std::string filename;
    filename.reserve(timestamp_.size() + 24);
    filename.append(timestamp_);
    if (index > 0) {
        filename.push_back('_');
        const auto sequence = std::to_string(index + 1);
        if (sequence.size() < 4) {
            filename.append(4 - sequence.size(), '0');
        }
        filename.append(sequence);
    }
    filename.append(".log");
    return output_directory_ / filename;
}

}
