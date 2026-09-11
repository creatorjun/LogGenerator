// src/main.cpp
#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/sample_log_import_service.hpp"
#include "application/stress_test_service.hpp"
#include "infrastructure/async_file_logger.hpp"
#include "infrastructure/csv_sample_log_source.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/runtime_paths.hpp"
#include "infrastructure/transport_factory.hpp"
#include "presentation/app.hpp"
#ifdef _WIN32
#include "infrastructure/windows_execution_runtime.hpp"
#include "presentation/windows_icon.hpp"
#include <Windows.h>
#else
#include "infrastructure/posix_execution_runtime.hpp"
#endif

#include <cstdio>
#include <exception>
#include <string>

#ifdef _WIN32
int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, const int show_command) {
    loggen::presentation::configure_application_identity();
#else
int main() {
#endif
    try {
        const auto paths = loggen::infrastructure::discover_runtime_paths();
        loggen::infrastructure::AsyncFileLogger logger{paths.log_directory};
        try {
            logger.info("LogGenerator startup");
            loggen::infrastructure::initialize_runtime_catalog(paths);
            loggen::infrastructure::JsonLogCatalog catalog;
            loggen::application::LogPreparationCache preparation_cache;
            loggen::application::LogCatalogService catalog_service{catalog, preparation_cache};
            loggen::infrastructure::CsvSampleLogSource sample_log_source;
            loggen::application::SampleLogImportService sample_log_import_service{sample_log_source, catalog_service};
            loggen::infrastructure::TransportFactory transport_factory{paths.generated_directory};
#ifdef _WIN32
            loggen::infrastructure::WindowsExecutionRuntime execution_runtime;
#else
            loggen::infrastructure::PosixExecutionRuntime execution_runtime;
#endif
            loggen::application::StressTestService stress_service{transport_factory, execution_runtime, preparation_cache, logger};
            loggen::presentation::App app{catalog_service, sample_log_import_service, logger, stress_service, paths.catalog_file, paths.generated_directory, paths.font_directory};
#ifdef _WIN32
            const int result = app.run(instance, show_command);
#else
            const int result = app.run();
#endif
            logger.info("LogGenerator shutdown completed");
            return result;
        } catch (const std::exception& error) {
            logger.critical(error.what());
#ifdef _WIN32
            loggen::presentation::show_application_error(instance, error.what());
#else
            std::fprintf(stderr, "LogGenerator: %s\n", error.what());
#endif
            return 1;
        }
    } catch (const std::exception& error) {
        const auto message = std::string("Application data initialization failed: ") + error.what();
#ifdef _WIN32
        loggen::presentation::show_application_error(instance, message);
#else
        std::fprintf(stderr, "LogGenerator: %s\n", message.c_str());
#endif
        return 1;
    }
}
