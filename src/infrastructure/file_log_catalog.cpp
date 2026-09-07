// src/infrastructure/file_log_catalog.cpp
#include "infrastructure/file_log_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace loggen::infrastructure {
namespace {

std::string normalized_extension(const std::filesystem::path& file) {
    auto extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

}

std::vector<domain::LogTemplate> FileLogCatalog::load(const std::filesystem::path& file) const {
    const auto extension = normalized_extension(file);
    if (extension == ".json") {
        return json_catalog_.load(file);
    }
    if (extension == ".csv") {
        return csv_catalog_.load(file);
    }
    throw std::invalid_argument("Unsupported catalog extension for '" + file.string() + "': expected .json or .csv");
}

void FileLogCatalog::save(const std::filesystem::path& file, const std::span<const domain::LogTemplate> items) const {
    const auto extension = normalized_extension(file);
    if (extension == ".json") {
        json_catalog_.save(file, items);
        return;
    }
    if (extension == ".csv") {
        csv_catalog_.save(file, items);
        return;
    }
    throw std::invalid_argument("Unsupported catalog extension for '" + file.string() + "': expected .json or .csv");
}

}
