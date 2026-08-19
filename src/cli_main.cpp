// src/cli_main.cpp
#include "application/log_catalog_service.hpp"
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/transport_factory.hpp"
#include "presentation/cli_app.hpp"

#ifdef _WIN32
#include "infrastructure/windows_execution_runtime.hpp"
#include <Windows.h>
#else
#include "infrastructure/posix_execution_runtime.hpp"
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include <algorithm>
#include <array>
#include <cstdint>
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

std::filesystem::path executable_directory() {
#ifdef _WIN32
    std::array<wchar_t, 32'768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#elif defined(__APPLE__)
    std::uint32_t buffer_size = 1024;
    std::vector<char> buffer(buffer_size);
    if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
        buffer.resize(buffer_size);
        if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
            return std::filesystem::current_path();
        }
    }
    std::error_code error;
    const auto executable = std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()), error);
    return (error ? std::filesystem::path(buffer.data()) : executable).parent_path();
#else
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::current_path() : executable.parent_path();
#endif
}

}

int main(const int argument_count, char** argument_values) {
#ifdef _WIN32
    const ConsoleUtf8Guard console_encoding;
#endif
    try {
        const auto application_directory = executable_directory();
        loggen::infrastructure::AsyncFileLogger logger{application_directory / "logs", "LogGeneratorCli"};
        loggen::infrastructure::JsonLogCatalog catalog;
        loggen::application::LogCatalogService catalog_service{catalog};
        const auto generated_directory = application_directory / "generated";
        loggen::infrastructure::TransportFactory transport_factory{generated_directory};
#ifdef _WIN32
        loggen::infrastructure::WindowsExecutionRuntime execution_runtime;
#else
        loggen::infrastructure::PosixExecutionRuntime execution_runtime;
#endif
        loggen::application::StressTestService stress_service{transport_factory, execution_runtime, logger};
        auto catalog_file = application_directory / "Sample Logs" / "sample_logs.json";
        if (!std::filesystem::exists(catalog_file)) {
            catalog_file = std::filesystem::current_path() / "Sample Logs" / "sample_logs.json";
        }
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
        loggen::presentation::CliApp app{catalog_service, stress_service, logger, std::move(catalog_file)};
        return app.run(arguments, executable_name);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "LogGeneratorCli: %s\n", error.what());
        return 1;
    }
}
