// src/presentation/cli_app.hpp
#pragma once

#include "application/ports/logger.hpp"
#include "application/use_cases/log_catalog.hpp"
#include "application/use_cases/stress_test.hpp"
#include "domain/generator_config.hpp"

#include <chrono>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::presentation {

enum class CliCommand {
    Help,
    List,
    Run
};

struct CliOptions {
    CliCommand command{CliCommand::Help};
    domain::GeneratorConfig config;
    std::filesystem::path catalog_file;
    std::vector<std::string> sample_ids;
    std::chrono::seconds duration{0};
    std::chrono::milliseconds status_interval{1000};
    bool all_samples{false};
    bool quiet{false};
};

class CliApp final {
public:
    CliApp(application::ILogCatalogUseCase& catalog_service, application::IStressTestUseCase& stress_service, application::ILogger& logger, std::filesystem::path default_catalog_file);

    int run(std::span<const std::string_view> arguments, std::string_view executable_name = "LogGeneratorCli");
    [[nodiscard]] static CliOptions parse_arguments(std::span<const std::string_view> arguments);
    static void print_help(std::string_view executable_name = "LogGeneratorCli");

private:
    int list_catalog(const CliOptions& options);
    int run_generator(CliOptions options);

    application::ILogCatalogUseCase& catalog_service_;
    application::IStressTestUseCase& stress_service_;
    application::ILogger& logger_;
    std::filesystem::path default_catalog_file_;
};

}
