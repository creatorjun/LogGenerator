// src/application/log_catalog_service.cpp
#include "application/log_catalog_service.hpp"

#include "application/privacy_anonymizer.hpp"
#include "domain/sample_id.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace {

void validate_ids(const std::span<const loggen::domain::LogTemplate> items) {
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(items.size());
    for (const auto& item : items) {
        if (!loggen::domain::valid_sample_id(item.id)) {
            throw std::invalid_argument("Sample id must contain only digits: '" + item.id + "'");
        }
        if (!identifiers.insert(item.id).second) {
            throw std::invalid_argument("Duplicate sample id: '" + item.id + "'");
        }
    }
}

}

namespace loggen::application {

LogCatalogService::LogCatalogService(ILogCatalog& catalog) noexcept
    : catalog_(catalog) {
}

std::vector<domain::LogTemplate> LogCatalogService::load(const std::filesystem::path& file) const {
    auto items = catalog_.load(file);
    validate_ids(items);
    for (auto& item : items) {
        item.sample = PrivacyAnonymizer::sanitize(item.sample);
    }
    return items;
}

void LogCatalogService::save(const std::filesystem::path& file, const std::span<const domain::LogTemplate> items) const {
    validate_ids(items);
    std::vector<domain::LogTemplate> sanitized{items.begin(), items.end()};
    for (auto& item : sanitized) {
        item.sample = PrivacyAnonymizer::sanitize(item.sample);
    }
    catalog_.save(file, sanitized);
}

std::string LogCatalogService::next_id(const std::span<const domain::LogTemplate> items) {
    validate_ids(items);
    std::uint64_t maximum = 0;
    for (const auto& item : items) {
        std::uint64_t value = 0;
        const auto [position, error] = std::from_chars(item.id.data(), item.id.data() + item.id.size(), value);
        if (error != std::errc{} || position != item.id.data() + item.id.size()) {
            throw std::overflow_error("Sample id is too large: '" + item.id + "'");
        }
        maximum = std::max(maximum, value);
    }
    if (maximum == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("No numeric sample id remains available");
    }
    auto result = std::to_string(maximum + 1);
    if (result.size() < 4) {
        result.insert(0, 4 - result.size(), '0');
    }
    return result;
}

}
