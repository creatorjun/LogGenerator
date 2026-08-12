// src/infrastructure/json_log_catalog.cpp
#include "infrastructure/json_log_catalog.hpp"

#include "domain/log_limits.hpp"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <fstream>
#include <format>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace loggen::infrastructure {
namespace {

using Json = nlohmann::json;

std::atomic<std::uint64_t> temporary_sequence{0};

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

JsonLogCatalog::JsonLogCatalog(std::filesystem::path file)
    : file_(std::move(file)) {
}

std::vector<domain::LogTemplate> JsonLogCatalog::load() const {
    std::error_code size_error;
    const auto file_size = std::filesystem::file_size(file_, size_error);
    if (size_error) {
        throw catalog_error(file_, "file size could not be read (" + size_error.message() + ")");
    }
    if (file_size > domain::maximum_catalog_file_bytes) {
        throw catalog_error(file_, "file exceeds the supported size");
    }
    std::ifstream input(file_, std::ios::binary);
    if (!input) {
        throw catalog_error(file_, "file was not found or could not be opened");
    }
    std::string payload(static_cast<std::size_t>(file_size), '\0');
    if (!payload.empty()) {
        input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (input.gcount() != static_cast<std::streamsize>(payload.size())) {
            throw catalog_error(file_, "file changed or ended while it was being read");
        }
    }
    char extra = '\0';
    if (input.get(extra)) {
        throw catalog_error(file_, "file changed while it was being read");
    }
    Json root;
    try {
        root = Json::parse(payload, nullptr, true, true);
    } catch (const std::exception& error) {
        throw catalog_error(file_, error.what());
    }
    if (!root.is_object() || root.value("schema_version", 0) != 1 || !root.contains("logs") || !root["logs"].is_array()) {
        throw catalog_error(file_, "schema_version 1 and a logs array are required");
    }
    if (root["logs"].size() > domain::maximum_log_template_count) {
        throw catalog_error(file_, "logs array exceeds the supported entry count");
    }

    std::vector<domain::LogTemplate> result;
    result.reserve(root["logs"].size());
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(root["logs"].size());
    for (const auto& value : root["logs"]) {
        if (!value.is_object()) {
            throw catalog_error(file_, "every logs entry must be an object");
        }
        domain::LogTemplate item;
        item.id = required_text(value, "id", file_);
        item.name = required_text(value, "name", file_);
        item.sample = required_text(value, "sample", file_);
        if (item.id.empty() || item.name.empty() || item.sample.empty()) {
            throw catalog_error(file_, "id, name and sample must not be empty");
        }
        if (item.id.size() > domain::maximum_log_identifier_bytes || item.name.size() > domain::maximum_log_name_bytes || item.sample.size() > domain::maximum_log_sample_bytes) {
            throw catalog_error(file_, "id, name or sample exceeds the supported size");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file_, "duplicate id '" + item.id + "'");
        }
        result.push_back(std::move(item));
    }
    return result;
}

void JsonLogCatalog::save(const std::span<const domain::LogTemplate> items) {
    if (items.size() > domain::maximum_log_template_count) {
        throw catalog_error(file_, "entry count exceeds the supported limit");
    }
    Json logs = Json::array();
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(items.size());
    for (const auto& item : items) {
        if (item.id.empty() || item.name.empty() || item.sample.empty()) {
            throw catalog_error(file_, "id, name and sample must not be empty");
        }
        if (item.id.size() > domain::maximum_log_identifier_bytes || item.name.size() > domain::maximum_log_name_bytes || item.sample.size() > domain::maximum_log_sample_bytes) {
            throw catalog_error(file_, "id, name or sample exceeds the supported size");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file_, "duplicate id '" + item.id + "'");
        }
        Json value{{"id", item.id}, {"name", item.name}, {"sample", item.sample}};
        logs.push_back(std::move(value));
    }
    const Json root{{"schema_version", 1}, {"logs", std::move(logs)}};
    if (!file_.parent_path().empty()) {
        std::filesystem::create_directories(file_.parent_path());
    }
    const auto serialized = root.dump(2, ' ', false, Json::error_handler_t::strict) + '\n';
    if (serialized.size() > domain::maximum_catalog_file_bytes) {
        throw catalog_error(file_, "serialized file exceeds the supported size");
    }
    auto temporary = file_;
    temporary += std::format(L".tmp.{}.{}.{}", GetCurrentProcessId(), GetCurrentThreadId(), temporary_sequence.fetch_add(1, std::memory_order_relaxed));
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                throw catalog_error(file_, "temporary file could not be opened");
            }
            output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
            output.flush();
            if (!output) {
                throw catalog_error(file_, "temporary file write failed");
            }
            output.close();
            if (!output) {
                throw catalog_error(file_, "temporary file close failed");
            }
        }
        if (!MoveFileExW(temporary.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            const auto error = GetLastError();
            throw catalog_error(file_, "atomic replacement failed (" + std::to_string(error) + ")");
        }
    } catch (...) {
        std::error_code cleanup_error;
        std::filesystem::remove(temporary, cleanup_error);
        throw;
    }
}

}
