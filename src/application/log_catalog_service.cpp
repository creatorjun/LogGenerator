// src/application/log_catalog_service.cpp
#include "application/log_catalog_service.hpp"

#include "application/privacy_anonymizer.hpp"

namespace loggen::application {

LogCatalogService::LogCatalogService(ILogCatalog& catalog) noexcept
    : catalog_(catalog) {
}

std::vector<domain::LogTemplate> LogCatalogService::load(const std::filesystem::path& file) const {
    auto items = catalog_.load(file);
    for (auto& item : items) {
        item.sample = PrivacyAnonymizer::sanitize(item.sample);
    }
    return items;
}

void LogCatalogService::save(const std::filesystem::path& file, const std::span<const domain::LogTemplate> items) const {
    std::vector<domain::LogTemplate> sanitized{items.begin(), items.end()};
    for (auto& item : sanitized) {
        item.sample = PrivacyAnonymizer::sanitize(item.sample);
    }
    catalog_.save(file, sanitized);
}

}
