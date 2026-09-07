// tests/csv_log_catalog_tests.cpp
#include "test_support.hpp"

#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/log_renderer.hpp"
#include "infrastructure/csv_log_catalog.hpp"
#include "infrastructure/file_log_catalog.hpp"

#include <chrono>
#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::tests {

void run_csv_log_catalog_tests() {
    const auto source = std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "privacy_demo_scenarios.csv";
    infrastructure::FileLogCatalog catalog;
    application::LogPreparationCache preparation_cache;
    application::LogCatalogService service{catalog, preparation_cache};
    const auto items = service.load(source);

    expect(items.size() == 5, "Privacy demo CSV must contain five scenarios");
    expect(items.front().id == "9001" && items.back().id == "9005", "Privacy demo CSV scenario ids are incorrect");
    expect(items[0].name.find("[메일]") != std::string::npos, "Split mail scenario name is missing");
    expect(items[1].name.find("[메일]") != std::string::npos, "Mail leak scenario name is missing");
    expect(items[2].name.find("[DB 접근제어]") != std::string::npos, "DB access-control scenario name is missing");
    expect(items[3].name.find("[출력물]") != std::string::npos, "Print scenario name is missing");
    expect(items[4].name.find("[PC DLP]") != std::string::npos, "PC DLP scenario name is missing");

    const auto render_time = std::chrono::sys_days{std::chrono::year{2030} / std::chrono::January / 2} + std::chrono::hours{3} + std::chrono::minutes{4} + std::chrono::seconds{5};
    std::vector<std::string> rendered;
    rendered.reserve(items.size());
    for (const auto& item : items) {
        const auto analysis = service.analyze(item);
        expect(analysis.timestamp_count == 1, "Privacy demo scenario timestamp was not recognized");
        expect(analysis.source_ip_count == 1, "Privacy demo scenario source IP was not recognized");
        expect(analysis.privacy_token_count > 0, "Privacy demo scenario has no privacy tokens");
        auto prepared = application::LogRenderer::prepare_one(item, "192.0.2.10", "192.0.2.20", std::chrono::seconds{0});
        rendered.emplace_back(prepared.render(render_time, true));
        expect(rendered.back().find("{{") == std::string::npos, "Privacy demo rendered output leaked a token marker");
        expect(rendered.back().find_first_of("\r\n") == std::string::npos, "Privacy demo rendered output contains a line break");
        expect(rendered.back().find("192.0.2.10") != std::string::npos, "Privacy demo rendered output lost the configured source IP");
    }

    expect(rendered[0].find("caseId=MAIL-SPLIT-001") != std::string::npos, "Split mail scenario correlation id is missing");
    expect(rendered[0].find("resident_front=900101") != std::string::npos, "Split mail scenario resident-number front fragment is malformed");
    expect(std::regex_search(rendered[0], std::regex{R"(resident_back=1[0-9]{6})"}), "Split mail scenario resident-number back fragment is malformed");
    expect(rendered[0].find("correlation=RECONSTRUCTED") != std::string::npos, "Split mail scenario correlation result is missing");
    expect(std::regex_search(rendered[1], std::regex{R"(resident_no=900101-1[0-9]{6})"}), "Mail leak scenario resident number is malformed");
    expect(rendered[1].find("recipientType=EXTERNAL") != std::string::npos, "Mail leak scenario external recipient evidence is missing");
    expect(rendered[2].find("SELECT customer_name,resident_no,mobile,email,address") != std::string::npos, "DB scenario sensitive query is missing");
    expect(rendered[2].find("returnedRows=25000") != std::string::npos, "DB scenario bulk-query evidence is missing");
    expect(rendered[3].find("document=고객_개인정보_현황.pdf") != std::string::npos, "Print scenario document evidence is missing");
    expect(rendered[3].find("action=HOLD") != std::string::npos, "Print scenario response is missing");
    expect(rendered[4].find("channel=USB") != std::string::npos, "PC DLP scenario removable-media evidence is missing");
    expect(rendered[4].find("action=BLOCK") != std::string::npos, "PC DLP scenario response is missing");

    const auto directory = unique_test_path("loggen_csv_catalog_");
    const auto csv_file = directory / "round_trip.csv";
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    std::vector<domain::LogTemplate> expected{
        {"9901", "쉼표, 따옴표 시나리오", "message=\"alpha,beta\" path={{FILE_PATH}}\ncontinued", "CSV round trip", {}},
    };
    expected.front().test_case.values["FILE_PATH"] = {"C:/Demo/quoted,\"file\".csv"};
    infrastructure::CsvLogCatalog csv_catalog;
    csv_catalog.save(csv_file, expected);
    const auto actual = csv_catalog.load(csv_file);
    expect(actual.size() == expected.size(), "CSV catalog round trip changed item count");
    expect(actual.front().id == expected.front().id, "CSV catalog round trip changed id");
    expect(actual.front().name == expected.front().name, "CSV catalog round trip changed UTF-8 name");
    expect(actual.front().source == expected.front().source, "CSV catalog round trip changed source");
    expect(actual.front().sample == expected.front().sample, "CSV catalog round trip changed quoted or multiline sample");
    expect(actual.front().test_case.values == expected.front().test_case.values, "CSV catalog round trip changed test-case JSON");

    bool unsupported_rejected = false;
    try {
        static_cast<void>(catalog.load(directory / "catalog.txt"));
    } catch (const std::invalid_argument&) {
        unsupported_rejected = true;
    }
    expect(unsupported_rejected, "File catalog accepted an unsupported extension");
    std::filesystem::remove_all(directory, cleanup_error);
}

}
