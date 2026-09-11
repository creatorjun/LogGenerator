// src/cli_main.cpp
#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/runtime_paths.hpp"
#include "infrastructure/transport_factory.hpp"
#include "presentation/cli_app.hpp"

#ifdef _WIN32
#include "infrastructure/windows_execution_runtime.hpp"
#include <Windows.h>
#else
#include "infrastructure/posix_execution_runtime.hpp"
#endif

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

#ifdef _WIN32
bool is_console_handle(const DWORD standard_handle) noexcept {
    const HANDLE handle = GetStdHandle(standard_handle);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD mode = 0;
    return GetConsoleMode(handle, &mode) != 0;
}

class ConsoleUtf8Guard final {
public:
    ConsoleUtf8Guard() noexcept {
        if (is_console_handle(STD_INPUT_HANDLE)) {
            input_code_page_ = GetConsoleCP();
            input_changed_ = input_code_page_ != 0 && input_code_page_ != CP_UTF8 && SetConsoleCP(CP_UTF8) != 0;
        }
        if (is_console_handle(STD_OUTPUT_HANDLE) || is_console_handle(STD_ERROR_HANDLE)) {
            output_code_page_ = GetConsoleOutputCP();
            output_changed_ = output_code_page_ != 0 && output_code_page_ != CP_UTF8 && SetConsoleOutputCP(CP_UTF8) != 0;
        }
    }

    ~ConsoleUtf8Guard() {
        std::fflush(stdout);
        std::fflush(stderr);
        if (output_changed_) {
            static_cast<void>(SetConsoleOutputCP(output_code_page_));
        }
        if (input_changed_) {
            static_cast<void>(SetConsoleCP(input_code_page_));
        }
    }

    ConsoleUtf8Guard(const ConsoleUtf8Guard&) = delete;
    ConsoleUtf8Guard& operator=(const ConsoleUtf8Guard&) = delete;

private:
    UINT input_code_page_{0};
    UINT output_code_page_{0};
    bool input_changed_{false};
    bool output_changed_{false};
};
#endif

}

int main(const int argument_count, char** argument_values) {
#ifdef _WIN32
    const ConsoleUtf8Guard console_encoding;
#endif
    try {
        const auto paths = loggen::infrastructure::discover_runtime_paths();
        loggen::infrastructure::AsyncFileLogger logger{paths.log_directory, "LogGeneratorCli"};
        loggen::infrastructure::initialize_runtime_catalog(paths);
        loggen::infrastructure::JsonLogCatalog catalog;
        loggen::application::LogPreparationCache preparation_cache;
        loggen::application::LogCatalogService catalog_service{catalog, preparation_cache};
        loggen::infrastructure::TransportFactory transport_factory{paths.generated_directory};
#ifdef _WIN32
        loggen::infrastructure::WindowsExecutionRuntime execution_runtime;
#else
        loggen::infrastructure::PosixExecutionRuntime execution_runtime;
#endif
        loggen::application::StressTestService stress_service{transport_factory, execution_runtime, preparation_cache, logger};
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(std::max(argument_count - 1, 0)));
        for (int index = 1; index < argument_count; ++index) {
            arguments.emplace_back(argument_values[index]);
        }
        auto executable_name = std::string{"LogGeneratorCli"};
        if (argument_count > 0 && argument_values[0] != nullptr) {
            const auto candidate = std::filesystem::path(argument_values[0]).filename().string();
            if (!candidate.empty()) {
                executable_name = candidate;
            }
        }
        loggen::presentation::CliApp app{catalog_service, stress_service, logger, paths.catalog_file};
        return app.run(arguments, executable_name);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "LogGeneratorCli: %s\n", error.what());
        return 1;
    }
}
