// tests/test_main.cpp
#include "test_support.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        loggen::tests::run_log_renderer_tests();
        loggen::tests::run_cli_app_tests();
        loggen::tests::run_log_catalog_service_tests();
        loggen::tests::run_json_log_catalog_tests();
        loggen::tests::run_file_transport_tests();
        loggen::tests::run_stress_test_service_tests();
        loggen::tests::run_async_file_logger_tests();
#ifdef LOGGEN_HAS_WINDOWS_ICON_TESTS
        loggen::tests::run_windows_icon_tests();
#endif
        loggen::tests::run_responsive_layout_tests();
        std::cout << "All LogGenerator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
