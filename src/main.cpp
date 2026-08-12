// src/main.cpp
#include "application/stress_test_service.hpp"
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
        loggen::infrastructure::ExcelLogCatalog catalog;
        loggen::infrastructure::TransportFactory transport_factory;
        loggen::application::StressTestService stress_service{transport_factory};
        auto sample_directory = executable_directory() / L"Sample Logs";
        if (!std::filesystem::exists(sample_directory)) {
            sample_directory = std::filesystem::current_path() / L"Sample Logs";
        }
        loggen::presentation::App app{catalog, stress_service, std::move(sample_directory)};
        return app.run(instance, show_command);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "LogGenerator", MB_OK | MB_ICONERROR);
        return 1;
    }
}
