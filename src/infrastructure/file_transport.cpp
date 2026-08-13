// src/infrastructure/file_transport.cpp
#include "infrastructure/file_transport.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace loggen::infrastructure {

FileTransport::FileTransport(std::filesystem::path output_directory)
    : output_directory_(std::move(output_directory)) {
}

FileTransport::~FileTransport() {
    close();
}

void FileTransport::connect(const domain::EndpointConfig& endpoint) {
    close();
    std::filesystem::create_directories(output_directory_);
    SYSTEMTIME value{};
    GetLocalTime(&value);
    timestamp_ = std::format(L"{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}", value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond, value.wMilliseconds);
    slice_index_ = 0;
    current_size_ = 0;
    total_size_ = 0;
    max_total_bytes_ = endpoint.file_max_total_bytes;
    max_file_count_ = endpoint.file_max_count;
    max_duration_ = endpoint.file_max_duration;
    started_at_ = std::chrono::steady_clock::now();
    open_next_slice();
}

application::SendResult FileTransport::send(const std::string_view payload) {
    if (file_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Generated log file is not open");
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
    if (current_size_ > 0 && current_size_ + payload.size() > slice_size_bytes) {
        if (max_file_count_ > 0 && slice_index_ + 1 >= max_file_count_) {
            return application::SendResult::FileCountLimitReached;
        }
        close();
        ++slice_index_;
        current_size_ = 0;
        open_next_slice();
    }

    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto remaining = payload.size() - offset;
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
        DWORD written = 0;
        if (!WriteFile(file_, payload.data() + offset, chunk, &written, nullptr)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log write failed");
        }
        if (written != chunk) {
            throw std::runtime_error("Generated log write was incomplete");
        }
        offset += written;
    }
    current_size_ += payload.size();
    total_size_ += payload.size();
    return application::SendResult::Sent;
}

bool FileTransport::is_datagram() const noexcept {
    return false;
}

void FileTransport::close() noexcept {
    if (file_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_);
        file_ = INVALID_HANDLE_VALUE;
    }
}

void FileTransport::open_next_slice() {
    for (;;) {
        const auto path = slice_path(slice_index_);
        file_ = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (file_ != INVALID_HANDLE_VALUE) {
            return;
        }
        const auto error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            throw std::system_error(static_cast<int>(error), std::system_category(), "Generated log file creation failed");
        }
        ++slice_index_;
    }
}

std::filesystem::path FileTransport::slice_path(const std::uint32_t index) const {
    if (index == 0) {
        return output_directory_ / std::format(L"{}.log", timestamp_);
    }
    return output_directory_ / std::format(L"{}_{:04}.log", timestamp_, index + 1);
}

}
