// src/main.cpp
#include "application/log_catalog_service.hpp"
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/transport_factory.hpp"
#include "infrastructure/timestamp_identifier_generator.hpp"
#include "infrastructure/windows_execution_runtime.hpp"
#include "presentation/app.hpp"

#include <Windows.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::filesystem::path executable_directory() {
    std::vector<wchar_t> buffer(32'768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int show_command) {
    try {
        const auto application_directory = executable_directory();
        loggen::infrastructure::AsyncFileLogger logger{application_directory / L"logs"};
        try {
            logger.info("LogGenerator startup");
            const auto generated_directory = application_directory / L"generated";
            loggen::infrastructure::TransportFactory transport_factory{generated_directory};
            loggen::infrastructure::WindowsExecutionRuntime execution_runtime;
            loggen::application::StressTestService stress_service{transport_factory, execution_runtime, logger};
            auto catalog_file = application_directory / L"Sample Logs" / L"sample_logs.json";
            if (!std::filesystem::exists(catalog_file)) {
                catalog_file = std::filesystem::current_path() / L"Sample Logs" / L"sample_logs.json";
            }
            loggen::infrastructure::JsonLogCatalog catalog{std::move(catalog_file)};
            loggen::infrastructure::TimestampIdentifierGenerator identifier_generator;
            loggen::application::LogCatalogService catalog_service{catalog, identifier_generator, logger};
            loggen::presentation::App app{catalog_service, logger, stress_service, generated_directory};
            const int result = app.run(instance, show_command);
            logger.info("LogGenerator shutdown completed");
            return result;
        } catch (const std::exception& error) {
            logger.critical(error.what());
            MessageBoxA(nullptr, error.what(), "LogGenerator", MB_OK | MB_ICONERROR);
            return 1;
        } catch (...) {
            logger.critical("Unknown fatal application error");
            MessageBoxW(nullptr, L"알 수 없는 치명적 오류로 LogGenerator를 종료합니다.", L"LogGenerator", MB_OK | MB_ICONERROR);
            return 1;
        }
    } catch (const std::exception& error) {
        const auto message = std::string("File logger initialization failed: ") + error.what();
        MessageBoxA(nullptr, message.c_str(), "LogGenerator", MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        MessageBoxW(nullptr, L"파일 로거 초기화 중 알 수 없는 오류가 발생했습니다.", L"LogGenerator", MB_OK | MB_ICONERROR);
        return 1;
    }
}
