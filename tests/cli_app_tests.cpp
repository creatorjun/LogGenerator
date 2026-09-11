// tests/cli_app_tests.cpp
#include "test_support.hpp"

#include "presentation/cli_app.hpp"
#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "infrastructure/json_log_catalog.hpp"

#include <array>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace loggen::tests {
namespace {

class EmptyCatalogFixture final {
public:
    EmptyCatalogFixture() : file(unique_test_path("loggen_empty_cli_catalog_")) {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << R"({"schema_version":1,"logs":[]})";
        output.close();
        expect(static_cast<bool>(output), "Unable to create empty Store catalog fixture");
    }

    ~EmptyCatalogFixture() {
        std::error_code error;
        std::filesystem::remove(file, error);
    }

    EmptyCatalogFixture(const EmptyCatalogFixture&) = delete;
    EmptyCatalogFixture& operator=(const EmptyCatalogFixture&) = delete;

    const std::filesystem::path file;
};

class EmptyCatalogLogger final : public application::ILogger {
public:
    void log(application::LogLevel, std::string_view) noexcept override {
    }
};

class EmptyCatalogStressService final : public application::IStressTestUseCase {
public:
    void start(domain::GeneratorConfig) override {
        started = true;
        throw std::logic_error("Empty catalog must not start a generator");
    }

    void request_stop() noexcept override {
    }

    void stop() noexcept override {
    }

    [[nodiscard]] domain::TransmissionStats snapshot() override {
        return {};
    }

    bool started{false};
};

void empty_store_catalog_is_listed_and_cannot_start_transmission() {
    const EmptyCatalogFixture fixture;
    infrastructure::JsonLogCatalog catalog;
    application::LogPreparationCache preparation_cache;
    application::LogCatalogService catalog_service{catalog, preparation_cache};
    EmptyCatalogStressService stress_service;
    EmptyCatalogLogger logger;
    presentation::CliApp app{catalog_service, stress_service, logger, fixture.file};
    constexpr std::array<std::string_view, 1> list_arguments{"list"};
    expect(app.run(list_arguments) == 0, "CLI must list an empty Store catalog successfully");
    constexpr std::array<std::string_view, 3> run_arguments{"run", "--all", "--quiet"};
    expect(app.run(run_arguments) == 2, "CLI must reject transmission from an empty Store catalog");
    expect(!stress_service.started, "An empty Store catalog started a transport");
    expect(catalog_service.load(fixture.file).empty(), "CLI must not populate an empty Store catalog with default samples");
}

}

void run_cli_app_tests() {
    empty_store_catalog_is_listed_and_cannot_start_transmission();
    using namespace std::chrono;
    constexpr std::array<std::string_view, 25> run_arguments{
        "run",
        "--protocol", "tls",
        "--host", "logs.example.test",
        "--port", "6514",
        "--tls-server-name", "siem.example.test",
        "--insecure",
        "--framing", "octet",
        "--sample-id", "0001",
        "--sample-id", "0002",
        "--mode", "sequential",
        "--eps", "1000",
        "--duration", "5",
        "--offset-minutes", "-90",
        "--quiet",
    };
    const auto run = presentation::CliApp::parse_arguments(run_arguments);
    expect(run.command == presentation::CliCommand::Run, "CLI run command was not parsed");
    expect(run.config.endpoint.protocol == domain::TransportProtocol::Tls, "CLI TLS protocol was not parsed");
    expect(run.config.endpoint.host == "logs.example.test" && run.config.endpoint.port == 6514, "CLI network endpoint was not parsed");
    expect(run.config.endpoint.tls_server_name == "siem.example.test" && !run.config.endpoint.verify_certificate, "CLI TLS options were not parsed");
    expect(run.config.endpoint.framing == domain::StreamFraming::OctetCounting, "CLI stream framing was not parsed");
    expect(run.sample_ids.size() == 2 && !run.all_samples, "CLI repeated sample identifiers were not parsed");
    expect(run.config.transmission_mode == domain::TransmissionMode::Sequential && run.config.target_eps == 1000, "CLI transmission mode or EPS option was not parsed");
    expect(run.duration == seconds{5} && run.quiet, "CLI duration or quiet option was not parsed");
    expect(run.config.timestamp_generation.offset.negative, "CLI negative offset sign was not parsed");
    expect(run.config.timestamp_generation.offset.hours == 1 && run.config.timestamp_generation.offset.minutes == 30, "CLI offset magnitude was not parsed");
    expect(run.config.endpoint.udp_packetization == domain::UdpPacketization::OneEventPerDatagram, "CLI integration default was not deny");

    constexpr std::array<std::string_view, 10> udp_arguments{"run", "--all", "--protocol", "udp", "--host", "127.0.0.1", "--port", "5514", "--udp-integration", "allow"};
    const auto udp = presentation::CliApp::parse_arguments(udp_arguments);
    expect(udp.config.endpoint.udp_packetization == domain::UdpPacketization::NewlinePacked, "CLI UDP integration permission was not parsed");

    constexpr std::array<std::string_view, 8> range_arguments{"run", "--all", "--from", "2026-01-01", "--to", "2026-01-31", "--file-max-count", "10"};
    const auto range = presentation::CliApp::parse_arguments(range_arguments);
    expect(range.config.timestamp_generation.mode == domain::TimestampGenerationMode::Range, "CLI timestamp range mode was not parsed");
    expect(range.config.timestamp_generation.range.inclusive_seconds() == 31ULL * 24ULL * 60ULL * 60ULL, "CLI timestamp range boundaries are incorrect");
    expect(range.config.endpoint.file_max_count == 10 && range.all_samples, "CLI FILE count limit or all switch was not parsed");

    constexpr std::array<std::string_view, 3> list_arguments{"list", "--catalog", "custom.json"};
    const auto list = presentation::CliApp::parse_arguments(list_arguments);
    expect(list.command == presentation::CliCommand::List && list.catalog_file == "custom.json", "CLI list command was not parsed");

    bool invalid_rejected = false;
    try {
        constexpr std::array<std::string_view, 3> invalid_arguments{"run", "--mode", "turbo"};
        static_cast<void>(presentation::CliApp::parse_arguments(invalid_arguments));
    } catch (const std::invalid_argument&) {
        invalid_rejected = true;
    }
    expect(invalid_rejected, "CLI accepted an invalid transmission mode");

    bool legacy_workers_rejected = false;
    try {
        constexpr std::array<std::string_view, 3> invalid_arguments{"run", "--workers", "4"};
        static_cast<void>(presentation::CliApp::parse_arguments(invalid_arguments));
    } catch (const std::invalid_argument&) {
        legacy_workers_rejected = true;
    }
    expect(legacy_workers_rejected, "CLI still accepted direct worker-count control");

    bool invalid_sample_id_rejected = false;
    try {
        constexpr std::array<std::string_view, 3> invalid_arguments{"run", "--sample-id", "sample-0001"};
        static_cast<void>(presentation::CliApp::parse_arguments(invalid_arguments));
    } catch (const std::invalid_argument&) {
        invalid_sample_id_rejected = true;
    }
    expect(invalid_sample_id_rejected, "CLI accepted a non-numeric sample id");

    bool conflicting_selection_rejected = false;
    try {
        constexpr std::array<std::string_view, 4> invalid_arguments{"run", "--all", "--sample-id", "0001"};
        static_cast<void>(presentation::CliApp::parse_arguments(invalid_arguments));
    } catch (const std::invalid_argument&) {
        conflicting_selection_rejected = true;
    }
    expect(conflicting_selection_rejected, "CLI accepted --all with --sample-id");

    bool missing_selection_rejected = false;
    try {
        constexpr std::array<std::string_view, 1> invalid_arguments{"run"};
        static_cast<void>(presentation::CliApp::parse_arguments(invalid_arguments));
    } catch (const std::invalid_argument&) {
        missing_selection_rejected = true;
    }
    expect(missing_selection_rejected, "CLI accepted run without --all or --sample-id");
}

}
