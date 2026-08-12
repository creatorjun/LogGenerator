// tests/test_support.hpp
#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace loggen::tests {

inline void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void run_log_renderer_tests();
void run_json_log_catalog_tests();
void run_file_transport_tests();
void run_stress_test_service_tests();
void run_async_file_logger_tests();
void run_responsive_layout_tests();

}
