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
void run_excel_log_catalog_tests();
void run_async_file_logger_tests();

}
