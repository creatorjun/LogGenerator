// src/application/sample_log_import_service.cpp
#include "application/sample_log_import_service.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace loggen::application {
namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

}

SampleLogImportService::SampleLogImportService(ISampleLogSource& source, ILogCatalogUseCase& catalog_service) noexcept
    : source_(source), catalog_service_(catalog_service) {
}

std::vector<TokenizedLogTemplate> SampleLogImportService::import_file(
    const std::filesystem::path& file,
    const std::span<const domain::LogTemplate> existing_items) const {
    const auto samples = source_.load(file);
    if (samples.empty()) {
        throw std::invalid_argument("CSV 파일에 가져올 샘플 로그가 없습니다.");
    }

    std::vector<domain::LogTemplate> assigned_items{existing_items.begin(), existing_items.end()};
    assigned_items.reserve(existing_items.size() + samples.size());
    std::vector<TokenizedLogTemplate> imported;
    imported.reserve(samples.size());

    auto base_name = path_to_utf8(file.stem());
    if (base_name.empty()) {
        base_name = "CSV 샘플 로그";
    }
    const auto source_name = "CSV 가져오기: " + path_to_utf8(file.filename());

    for (std::size_t index = 0; index < samples.size(); ++index) {
        domain::LogTemplate item;
        item.id = catalog_service_.next_id(assigned_items);
        item.name = base_name + " #" + std::to_string(index + 1);
        item.sample = samples[index];
        item.source = source_name;
        auto tokenized = catalog_service_.tokenize(std::move(item));
        assigned_items.push_back(tokenized.item);
        imported.push_back(std::move(tokenized));
    }
    return imported;
}

}
