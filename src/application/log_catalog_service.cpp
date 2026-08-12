// src/application/log_catalog_service.cpp
#include "application/log_catalog_service.hpp"

#include "application/log_renderer.hpp"
#include "application/privacy_anonymizer.hpp"
#include "domain/log_limits.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>
#include <utility>

namespace loggen::application {
namespace {

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string search_text(const domain::LogTemplate& item, const LogTemplateAnalysis& analysis) {
    std::string result;
    result.reserve(item.name.size() + item.sample.size() + 256);
    result.append(item.name);
    result.push_back(' ');
    result.append(item.sample);
    for (const auto kind : privacy_token_kinds) {
        if ((analysis.privacy_token_mask & privacy_token_bit(kind)) == 0) {
            continue;
        }
        result.push_back(' ');
        result.append(PrivacyAnonymizer::search_terms(kind));
    }
    return lowercase(std::move(result));
}

}

LogCatalogService::LogCatalogService(ILogCatalog& catalog, IIdentifierGenerator& identifier_generator, ILogger& logger)
    : catalog_(catalog), identifier_generator_(identifier_generator), logger_(logger) {
}

std::vector<CatalogItem> LogCatalogService::load() const {
    auto items = catalog_.load();
    std::vector<CatalogItem> result;
    result.reserve(items.size());
    for (auto& item : items) {
        result.push_back(describe(std::move(item)));
    }
    try {
        logger_.info(std::format("Sample log catalog loaded: entries={}", result.size()));
    } catch (...) {
        logger_.info("Sample log catalog loaded");
    }
    return result;
}

void LogCatalogService::save(const std::span<const domain::LogTemplate> items) {
    catalog_.save(items);
    try {
        logger_.info(std::format("Sample log catalog saved: entries={}", items.size()));
    } catch (...) {
        logger_.info("Sample log catalog saved");
    }
}

CatalogItem LogCatalogService::create(const std::span<const domain::LogTemplate> existing, std::string name, std::string sample) {
    for (std::size_t attempt = 0; attempt < 1024; ++attempt) {
        auto identifier = identifier_generator_.next("user");
        if (std::ranges::none_of(existing, [&identifier](const domain::LogTemplate& item) { return item.id == identifier; })) {
            return describe(domain::LogTemplate{std::move(identifier), std::move(name), std::move(sample)});
        }
    }
    throw std::runtime_error("Unable to generate a unique catalog identifier");
}

CatalogItem LogCatalogService::update(const domain::LogTemplate& existing, std::string name, std::string sample) const {
    return describe(domain::LogTemplate{existing.id, std::move(name), std::move(sample)});
}

CatalogItem LogCatalogService::describe(domain::LogTemplate item) const {
    if (item.id.empty() || item.name.empty() || item.sample.empty()) {
        throw std::invalid_argument("Catalog id, name and sample must not be empty");
    }
    if (item.id.size() > domain::maximum_log_identifier_bytes || item.name.size() > domain::maximum_log_name_bytes || item.sample.size() > domain::maximum_log_sample_bytes) {
        throw std::length_error("Catalog id, name or sample exceeds the supported size");
    }
    item.sample = PrivacyAnonymizer::sanitize(item.sample);
    auto analysis = LogRenderer::analyze_sanitized(item.sample);
    auto searchable = search_text(item, analysis);
    return CatalogItem{std::move(item), std::move(searchable), std::move(analysis)};
}

LogTemplateAnalysis LogCatalogService::analyze(const std::string_view sample) const {
    if (sample.size() > domain::maximum_log_sample_bytes) {
        throw std::length_error("Log sample exceeds the supported size");
    }
    return LogRenderer::analyze(sample);
}

}
