// src/infrastructure/file_transport.cpp
#include "infrastructure/file_transport.hpp"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace loggen::infrastructure {
namespace {

class FileCollision final : public std::exception {
};

class FileHandle final {
public:
    explicit FileHandle(const HANDLE value) noexcept
        : value_(value) {
    }

    ~FileHandle() {
        static_cast<void>(close());
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return value_;
    }

    [[nodiscard]] bool close() noexcept {
        if (value_ == INVALID_HANDLE_VALUE) {
            return true;
        }
        const auto value = value_;
        value_ = INVALID_HANDLE_VALUE;
        return CloseHandle(value) != FALSE;
    }

private:
    HANDLE value_{INVALID_HANDLE_VALUE};
};

}

FileTransport::FileTransport(std::filesystem::path output_directory)
    : output_directory_(std::move(output_directory)) {
}

FileTransport::~FileTransport() = default;

void FileTransport::connect(const domain::EndpointConfig& endpoint) {
    timestamp_.clear();
    std::filesystem::create_directories(output_directory_);
    SYSTEMTIME value{};
    GetLocalTime(&value);
    timestamp_ = std::format(L"{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}", value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond, value.wMilliseconds);
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
    const auto raw_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (raw_file == INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
            throw FileCollision{};
        }
        throw std::system_error(static_cast<int>(error), std::system_category(), "Generated log file creation failed");
    }

    FileHandle file{raw_file};
    try {
        std::size_t offset = 0;
        while (offset < log.size()) {
            const auto remaining = log.size() - offset;
            const auto chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
            DWORD written = 0;
            if (!WriteFile(file.get(), log.data() + offset, chunk, &written, nullptr)) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log write failed");
            }
            if (written != chunk) {
                throw std::runtime_error("Generated log write was incomplete");
            }
            offset += written;
        }
        if (!file.close()) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log close failed");
        }
    } catch (...) {
        static_cast<void>(file.close());
        std::error_code cleanup_error;
        std::filesystem::remove(path, cleanup_error);
        throw;
    }
}

std::filesystem::path FileTransport::file_path(const std::uint64_t index) const {
    std::wstring filename;
    filename.reserve(timestamp_.size() + 24);
    filename.append(timestamp_);
    if (index > 0) {
        filename.push_back(L'_');
        const auto sequence = std::to_wstring(index + 1);
        if (sequence.size() < 4) {
            filename.append(4 - sequence.size(), L'0');
        }
        filename.append(sequence);
    }
    filename.append(L".log");
    return output_directory_ / filename;
}

}
