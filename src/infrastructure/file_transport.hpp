// src/infrastructure/file_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace loggen::infrastructure {

class FileTransport final : public application::ILogTransport {
public:
    explicit FileTransport(std::filesystem::path output_directory);
    ~FileTransport() override;

    FileTransport(const FileTransport&) = delete;
    FileTransport& operator=(const FileTransport&) = delete;

    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    void write_log(const std::filesystem::path& path, std::string_view log);
    [[nodiscard]] std::filesystem::path file_path(std::uint64_t index) const;

    std::filesystem::path output_directory_;
    std::string timestamp_;
    std::uint64_t next_file_index_{0};
    std::uint64_t file_count_{0};
    std::uint64_t total_size_{0};
    std::uint64_t max_total_bytes_{0};
    std::uint32_t max_file_count_{0};
    std::chrono::milliseconds max_duration_{0};
    std::chrono::steady_clock::time_point started_at_{};
};

}
