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

void FileTransport::connect(const domain::EndpointConfig&) {
    close_checked();
    std::filesystem::create_directories(output_directory_);
    SYSTEMTIME value{};
    GetLocalTime(&value);
    timestamp_ = std::format(L"{:04}{:02}{:02}_{:02}{:02}{:02}_{:03}", value.wYear, value.wMonth, value.wDay, value.wHour, value.wMinute, value.wSecond, value.wMilliseconds);
    slice_index_ = 0;
    current_size_ = 0;
    open_next_slice();
}

void FileTransport::send(const std::string_view payload) {
    if (file_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Generated log file is not open");
    }
    if (payload.empty()) {
        return;
    }
    std::size_t offset = 0;
    while (offset < payload.size()) {
        if (current_size_ == slice_size_bytes) {
            close_checked();
            if (slice_index_ == std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("Generated log slice index limit was reached");
            }
            ++slice_index_;
            current_size_ = 0;
            open_next_slice();
        }
        const auto remaining = payload.size() - offset;
        const auto available = slice_size_bytes - current_size_;
        const auto chunk = static_cast<DWORD>(std::min<std::uint64_t>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()), available));
        DWORD written = 0;
        if (!WriteFile(file_, payload.data() + offset, chunk, &written, nullptr)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log write failed");
        }
        if (written == 0) {
            throw std::runtime_error("Generated log write made no progress");
        }
        offset += written;
        current_size_ += written;
    }
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

void FileTransport::close_checked() {
    if (file_ == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!CloseHandle(file_)) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Generated log file close failed");
    }
    file_ = INVALID_HANDLE_VALUE;
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
        if (slice_index_ == std::numeric_limits<std::uint32_t>::max()) {
            throw std::overflow_error("Generated log slice index limit was reached");
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
