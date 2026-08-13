// src/infrastructure/file_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"

#include <Windows.h>

#include <cstddef>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace loggen::infrastructure {

class FileTransport final : public application::ILogTransport {
public:
    static constexpr std::uint64_t slice_size_bytes = 1024ULL * 1024ULL;

    explicit FileTransport(std::filesystem::path output_directory);
    ~FileTransport() override;

    FileTransport(const FileTransport&) = delete;
    FileTransport& operator=(const FileTransport&) = delete;

    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    void close() noexcept;
    void open_next_slice();
    [[nodiscard]] std::filesystem::path slice_path(std::uint32_t index) const;

    std::filesystem::path output_directory_;
    HANDLE file_{INVALID_HANDLE_VALUE};
    std::wstring timestamp_;
    std::uint32_t slice_index_{0};
    std::uint64_t current_size_{0};
    std::uint64_t total_size_{0};
    std::uint64_t max_total_bytes_{0};
    std::uint32_t max_file_count_{0};
    std::chrono::milliseconds max_duration_{0};
    std::chrono::steady_clock::time_point started_at_{};
};

}
