// tests/test_main.cpp
#include "test_support.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        loggen::tests::run_log_renderer_tests();
        loggen::tests::run_excel_log_catalog_tests();
        loggen::tests::run_async_file_logger_tests();
        loggen::tests::run_responsive_layout_tests();
        std::cout << "All LogGenerator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
