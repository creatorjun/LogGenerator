// src/main.cpp
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/excel_log_catalog.hpp"
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
            loggen::infrastructure::ExcelLogCatalog catalog;
            loggen::infrastructure::TransportFactory transport_factory;
            loggen::application::StressTestService stress_service{transport_factory, logger};
            auto sample_directory = application_directory / L"Sample Logs";
            if (!std::filesystem::exists(sample_directory)) {
                sample_directory = std::filesystem::current_path() / L"Sample Logs";
            }
            loggen::presentation::App app{catalog, logger, stress_service, std::move(sample_directory)};
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
