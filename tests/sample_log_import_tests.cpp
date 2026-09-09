// tests/sample_log_import_tests.cpp
#include "test_support.hpp"

#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/ports/log_catalog.hpp"
#include "application/sample_log_import_service.hpp"
#include "infrastructure/csv_sample_log_source.hpp"

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace loggen::tests {
namespace {

class NullLogCatalog final : public application::ILogCatalog {
public:
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path&) const override {
        return {};
    }

    void save(const std::filesystem::path&, std::span<const domain::LogTemplate>) const override {
    }
};

class DirectoryGuard {
public:
    explicit DirectoryGuard(std::filesystem::path directory)
        : directory_(std::move(directory)) {
    }

    ~DirectoryGuard() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

private:
    std::filesystem::path directory_;
};

void write_text(const std::filesystem::path& file, const std::string_view content) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw std::runtime_error("Test CSV could not be written");
    }
}

bool load_rejected(const infrastructure::CsvSampleLogSource& source, const std::filesystem::path& file) {
    try {
        static_cast<void>(source.load(file));
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

}

void run_sample_log_import_tests() {
    const auto directory = unique_test_path("loggen_sample_log_import_");
    DirectoryGuard guard{directory};
    const auto source_file = directory / "privacy_demo.CSV";
    std::string source_content{"\xEF\xBB\xBF"};
    source_content.append("sample_log\r\n");
    source_content.append("\"timestamp=2026-09-09T10:20:30Z user_email=\"\"demo@example.com\"\",message=\"\"alpha,beta\"\"\"\r\n");
    source_content.append("\"line one\r\nline two\"\r\n");
    write_text(source_file, source_content);

    infrastructure::CsvSampleLogSource source;
    const auto raw_samples = source.load(source_file);
    expect(raw_samples.size() == 2, "Single-column CSV source did not load every sample row");
    expect(raw_samples[0].find("alpha,beta") != std::string::npos, "CSV source changed a quoted comma");
    expect(raw_samples[0].find("\"demo@example.com\"") != std::string::npos, "CSV source changed escaped quotes");
    expect(raw_samples[1] == "line one\nline two", "CSV source changed a quoted multiline sample");

    NullLogCatalog catalog;
    application::LogPreparationCache preparation_cache;
    application::LogCatalogService catalog_service{catalog, preparation_cache};
    application::SampleLogImportService import_service{source, catalog_service};
    const std::vector<domain::LogTemplate> existing{{"0007", "Existing", "event=existing", "test", {}}};
    const auto imported = import_service.import_file(source_file, existing);
    domain::LogTemplate manually_entered{"0008", "Manual", raw_samples.front(), "사용자 정의", {}};
    const auto manual_result = catalog_service.tokenize(std::move(manually_entered));
    expect(imported.size() == 2, "Import use case did not create one catalog item per CSV row");
    expect(imported[0].item.id == "0008" && imported[1].item.id == "0009", "Import use case did not allocate sequential catalog ids");
    expect(imported[0].item.name == "privacy_demo #1" && imported[1].item.name == "privacy_demo #2", "Import use case did not create stable sample names");
    expect(imported[0].item.source == "CSV 가져오기: privacy_demo.CSV", "Import use case did not retain CSV source metadata");
    expect(imported[0].item.sample == manual_result.item.sample, "CSV import did not match manual sample tokenization");
    expect(imported[0].item.sample.find("{{EMAIL}}") != std::string::npos, "Imported sample did not follow manual privacy tokenization");
    expect(imported[0].analysis.timestamp_count == 1, "Imported sample did not follow manual timestamp analysis");
    expect(imported[0].analysis.privacy_token_count >= 1, "Imported sample did not follow manual privacy analysis");

    const auto headerless_file = directory / "headerless.csv";
    write_text(headerless_file, "first log\nsecond log\n");
    const auto headerless = source.load(headerless_file);
    expect(headerless.size() == 2 && headerless.front() == "first log", "Headerless CSV lost its first sample row");

    const auto korean_header_file = directory / "korean_header.csv";
    write_text(korean_header_file, "샘플로그\n보안 이벤트\n");
    const auto korean_header = source.load(korean_header_file);
    expect(korean_header.size() == 1 && korean_header.front() == "보안 이벤트", "Optional Korean CSV header was imported as a sample");

    const auto multi_column_file = directory / "multi_column.csv";
    write_text(multi_column_file, "sample_log,other\nfirst,second\n");
    expect(load_rejected(source, multi_column_file), "CSV source accepted more than one column");

    const auto malformed_file = directory / "malformed.csv";
    write_text(malformed_file, "sample_log\n\"unterminated\n");
    expect(load_rejected(source, malformed_file), "CSV source accepted an unterminated quoted field");

    const auto empty_sample_file = directory / "empty_sample.csv";
    write_text(empty_sample_file, "sample_log\n\"\"\n");
    expect(load_rejected(source, empty_sample_file), "CSV source accepted an empty sample row");

    const auto wrong_extension_file = directory / "samples.txt";
    write_text(wrong_extension_file, "sample log\n");
    expect(load_rejected(source, wrong_extension_file), "CSV source accepted a non-CSV extension");
}

}
