// src/infrastructure/json_log_catalog.cpp
#include "infrastructure/json_log_catalog.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace loggen::infrastructure {
namespace {

using Json = nlohmann::json;

std::runtime_error catalog_error(const std::filesystem::path& file, const std::string& message) {
    return std::runtime_error("JSON catalog " + file.string() + ": " + message);
}

std::string required_text(const Json& value, const char* key, const std::filesystem::path& file) {
    if (!value.contains(key) || !value[key].is_string()) {
        throw catalog_error(file, std::string("missing string field '") + key + "'");
    }
    return value[key].get<std::string>();
}

}

std::vector<domain::LogTemplate> JsonLogCatalog::load(const std::filesystem::path& file) const {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw catalog_error(file, "file was not found or could not be opened");
    }
    Json root;
    try {
        root = Json::parse(input, nullptr, true, true);
    } catch (const std::exception& error) {
        throw catalog_error(file, error.what());
    }
    if (!root.is_object() || root.value("schema_version", 0) != 1 || !root.contains("logs") || !root["logs"].is_array()) {
        throw catalog_error(file, "schema_version 1 and a logs array are required");
    }

    std::vector<domain::LogTemplate> result;
    result.reserve(root["logs"].size());
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(root["logs"].size());
    for (const auto& value : root["logs"]) {
        if (!value.is_object()) {
            throw catalog_error(file, "every logs entry must be an object");
        }
        domain::LogTemplate item;
        item.id = required_text(value, "id", file);
        item.name = required_text(value, "name", file);
        item.sample = required_text(value, "sample", file);
        item.source = value.value("source", std::string{});
        if (item.id.empty() || item.name.empty() || item.sample.empty()) {
            throw catalog_error(file, "id, name and sample must not be empty");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file, "duplicate id '" + item.id + "'");
        }
        result.push_back(std::move(item));
    }
    return result;
}

void JsonLogCatalog::save(const std::filesystem::path& file, const std::span<const domain::LogTemplate> items) const {
    Json logs = Json::array();
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(items.size());
    for (const auto& item : items) {
        if (item.id.empty() || item.name.empty() || item.sample.empty()) {
            throw catalog_error(file, "id, name and sample must not be empty");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file, "duplicate id '" + item.id + "'");
        }
        Json value{{"id", item.id}, {"name", item.name}, {"sample", item.sample}};
        if (!item.source.empty()) {
            value["source"] = item.source;
        }
        logs.push_back(std::move(value));
    }
    const Json root{{"schema_version", 1}, {"logs", std::move(logs)}};
    std::filesystem::create_directories(file.parent_path());
    auto temporary = file;
    temporary += L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw catalog_error(file, "temporary file could not be opened");
        }
        output << root.dump(2, ' ', false, Json::error_handler_t::strict) << '\n';
        output.flush();
        if (!output) {
            throw catalog_error(file, "temporary file write failed");
        }
    }
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = GetLastError();
        std::error_code cleanup_error;
        std::filesystem::remove(temporary, cleanup_error);
        throw catalog_error(file, "atomic replacement failed (" + std::to_string(error) + ")");
    }
}

}
