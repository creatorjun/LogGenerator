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
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path executable_directory() {
#ifdef _WIN32
    std::array<wchar_t, 32'768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#else
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::current_path() : executable.parent_path();
#endif
}

}

int main(const int argument_count, char** argument_values) {
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
        loggen::presentation::CliApp app{catalog_service, stress_service, logger, std::move(catalog_file)};
        return app.run(arguments);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "LogGeneratorCli: %s\n", error.what());
        return 1;
    }
}
