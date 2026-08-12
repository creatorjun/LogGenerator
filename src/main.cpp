// src/main.cpp
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/transport_factory.hpp"
#include "presentation/app.hpp"

#include <Windows.h>

#include <array>
#include <exception>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path executable_directory() {
    std::array<wchar_t, 32'768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

}

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, const int show_command) {
    try {
        const auto application_directory = executable_directory();
        loggen::infrastructure::AsyncFileLogger logger{application_directory / L"logs"};
        try {
            logger.info("LogGenerator startup");
            loggen::infrastructure::JsonLogCatalog catalog;
            const auto generated_directory = application_directory / L"generated";
            loggen::infrastructure::TransportFactory transport_factory{generated_directory};
            loggen::application::StressTestService stress_service{transport_factory, logger};
            auto catalog_file = application_directory / L"Sample Logs" / L"sample_logs.json";
            if (!std::filesystem::exists(catalog_file)) {
                catalog_file = std::filesystem::current_path() / L"Sample Logs" / L"sample_logs.json";
            }
            loggen::presentation::App app{catalog, logger, stress_service, std::move(catalog_file), generated_directory};
            const int result = app.run(instance, show_command);
            logger.info("LogGenerator shutdown completed");
            return result;
        } catch (const std::exception& error) {
            logger.critical(error.what());
            MessageBoxA(nullptr, error.what(), "LogGenerator", MB_OK | MB_ICONERROR);
            return 1;
        }
    } catch (const std::exception& error) {
        const auto message = std::string("File logger initialization failed: ") + error.what();
        MessageBoxA(nullptr, message.c_str(), "LogGenerator", MB_OK | MB_ICONERROR);
        return 1;
    }
}
