// src/infrastructure/json_log_catalog.cpp
#include "infrastructure/json_log_catalog.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
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

domain::LogTestCase read_test_case(const Json& value, const std::filesystem::path& file) {
    domain::LogTestCase result;
    if (!value.contains("test_case")) {
        return result;
    }
    const auto& test_case = value["test_case"];
    if (!test_case.is_object()) {
        throw catalog_error(file, "test_case must be an object");
    }
    for (const auto& [token, values] : test_case.items()) {
        if (token.empty() || !values.is_array()) {
            throw catalog_error(file, "test_case values must be arrays keyed by token name");
        }
        auto& destination = result.values[token];
        destination.reserve(values.size());
        for (const auto& item : values) {
            if (!item.is_string()) {
                throw catalog_error(file, "every test_case value must be a string");
            }
            destination.push_back(item.get<std::string>());
        }
    }
    return result;
}

std::filesystem::path temporary_path_for(const std::filesystem::path& file) {
    static std::atomic_uint64_t sequence{0};
    auto temporary = file;
#ifdef _WIN32
    const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const auto process_id = static_cast<unsigned long>(getpid());
#endif
    temporary += ".tmp." + std::to_string(process_id) + "." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    return temporary;
}

bool replace_file_with_retry(const std::filesystem::path& source, const std::filesystem::path& destination, int& final_error) {
#ifdef _WIN32
    constexpr DWORD retryable_errors[]{ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION, ERROR_LOCK_VIOLATION};
    for (DWORD attempt = 0; attempt < 7; ++attempt) {
        if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            final_error = 0;
            return true;
        }
        final_error = static_cast<int>(GetLastError());
        bool retryable = false;
        for (const auto error : retryable_errors) {
            retryable = retryable || final_error == static_cast<int>(error);
        }
        if (!retryable || attempt == 6) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1U << attempt});
    }
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    final_error = error.value();
    return !error;
#endif
    return false;
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
        item.test_case = read_test_case(value, file);
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
        if (!item.test_case.values.empty()) {
            Json test_case = Json::object();
            for (const auto& [token, values] : item.test_case.values) {
                test_case[token] = values;
            }
            value["test_case"] = std::move(test_case);
        }
        logs.push_back(std::move(value));
    }
    const Json root{{"schema_version", 1}, {"logs", std::move(logs)}};
    std::filesystem::create_directories(file.parent_path());
    const auto temporary = temporary_path_for(file);
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
    int error = 0;
    if (!replace_file_with_retry(temporary, file, error)) {
        std::error_code cleanup_error;
        std::filesystem::remove(temporary, cleanup_error);
        throw catalog_error(file, "atomic replacement failed (" + std::to_string(error) + ")");
    }
}

}
