// tests/test_main.cpp
#include "test_support.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        loggen::tests::run_log_renderer_tests();
        loggen::tests::run_json_log_catalog_tests();
        loggen::tests::run_file_transport_tests();
        loggen::tests::run_stress_test_service_tests();
        loggen::tests::run_transport_error_tests();
        loggen::tests::run_async_file_logger_tests();
        loggen::tests::run_responsive_layout_tests();
        loggen::tests::run_log_catalog_service_tests();
        loggen::tests::run_generator_config_validator_tests();
        loggen::tests::run_catalog_task_runner_tests();
        loggen::tests::run_architecture_tests();
        std::cout << "All LogGenerator tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Test failure: unknown exception\n";
        return 1;
    }
}
