// tests/test_support.hpp
#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace loggen::tests {

inline void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

inline std::filesystem::path unique_test_path(const std::string_view prefix) {
    static std::atomic_uint64_t sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / (std::string(prefix) + std::to_string(timestamp) + '_' + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
}

void run_log_renderer_tests();
void run_cli_app_tests();
void run_log_catalog_service_tests();
void run_json_log_catalog_tests();
void run_file_transport_tests();
void run_udp_transport_tests();
void run_stress_test_service_tests();
void run_async_file_logger_tests();
#ifdef LOGGEN_HAS_WINDOWS_ICON_TESTS
void run_windows_icon_tests();
#endif
void run_responsive_layout_tests();

}
