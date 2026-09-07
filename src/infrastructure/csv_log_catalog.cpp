// src/infrastructure/csv_log_catalog.cpp
#include "infrastructure/csv_log_catalog.hpp"

#include "domain/sample_id.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace loggen::infrastructure {
namespace {

using Json = nlohmann::json;
using CsvRow = std::vector<std::string>;

std::runtime_error catalog_error(const std::filesystem::path& file, const std::string& message) {
    return std::runtime_error("CSV catalog " + file.string() + ": " + message);
}

std::vector<CsvRow> parse_csv(const std::string_view input, const std::filesystem::path& file) {
    std::vector<CsvRow> rows;
    CsvRow row;
    std::string field;
    bool quoted = false;
    bool quote_closed = false;
    bool row_started = false;

    const auto finish_field = [&] {
        row.push_back(std::move(field));
        field.clear();
        quote_closed = false;
    };
    const auto finish_row = [&] {
        if (row_started || !field.empty() || !row.empty() || quote_closed) {
            finish_field();
            rows.push_back(std::move(row));
            row.clear();
        }
        row_started = false;
        quote_closed = false;
    };

    for (std::size_t index = 0; index < input.size(); ++index) {
        const char value = input[index];
        if (value == '\0') {
            throw catalog_error(file, "NUL bytes are not allowed");
        }
        if (quoted) {
            if (value == '"') {
                if (index + 1 < input.size() && input[index + 1] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                    quote_closed = true;
                }
            } else if (value == '\r') {
                if (index + 1 < input.size() && input[index + 1] == '\n') {
                    ++index;
                }
                field.push_back('\n');
            } else {
                field.push_back(value);
            }
            continue;
        }
        if (quote_closed) {
            if (value == ',') {
                finish_field();
                row_started = true;
                continue;
            }
            if (value == '\r' || value == '\n') {
                if (value == '\r' && index + 1 < input.size() && input[index + 1] == '\n') {
                    ++index;
                }
                finish_row();
                continue;
            }
            throw catalog_error(file, "unexpected character after a closing quote");
        }
        if (value == '"') {
            if (!field.empty()) {
                throw catalog_error(file, "a quote must begin at the start of a field");
            }
            quoted = true;
            row_started = true;
        } else if (value == ',') {
            finish_field();
            row_started = true;
        } else if (value == '\r' || value == '\n') {
            if (value == '\r' && index + 1 < input.size() && input[index + 1] == '\n') {
                ++index;
            }
            finish_row();
        } else {
            field.push_back(value);
            row_started = true;
        }
    }
    if (quoted) {
        throw catalog_error(file, "unterminated quoted field");
    }
    finish_row();
    return rows;
}

std::unordered_map<std::string, std::size_t> read_header(const CsvRow& header, const std::filesystem::path& file) {
    static const std::unordered_set<std::string> allowed{"id", "name", "source", "sample", "test_case"};
    std::unordered_map<std::string, std::size_t> columns;
    columns.reserve(header.size());
    for (std::size_t index = 0; index < header.size(); ++index) {
        std::string name = header[index];
        std::ranges::transform(name, name.begin(), [](const unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (!allowed.contains(name)) {
            throw catalog_error(file, "unsupported header '" + header[index] + "'");
        }
        if (!columns.emplace(std::move(name), index).second) {
            throw catalog_error(file, "duplicate header '" + header[index] + "'");
        }
    }
    for (const auto required : {"id", "name", "sample"}) {
        if (!columns.contains(required)) {
            throw catalog_error(file, std::string("missing required header '") + required + "'");
        }
    }
    return columns;
}

const std::string& column_value(const CsvRow& row, const std::unordered_map<std::string, std::size_t>& columns, const std::string_view name) {
    return row[columns.at(std::string(name))];
}

domain::LogTestCase parse_test_case(const std::string& input, const std::filesystem::path& file, const std::size_t row_number) {
    domain::LogTestCase result;
    if (input.empty()) {
        return result;
    }
    Json value;
    try {
        value = Json::parse(input);
    } catch (const std::exception& error) {
        throw catalog_error(file, "row " + std::to_string(row_number) + " has invalid test_case JSON: " + error.what());
    }
    if (!value.is_object()) {
        throw catalog_error(file, "row " + std::to_string(row_number) + " test_case must be a JSON object");
    }
    for (const auto& [token, values] : value.items()) {
        if (token.empty() || !values.is_array()) {
            throw catalog_error(file, "row " + std::to_string(row_number) + " test_case values must be arrays keyed by token name");
        }
        auto& destination = result.values[token];
        destination.reserve(values.size());
        for (const auto& item : values) {
            if (!item.is_string()) {
                throw catalog_error(file, "row " + std::to_string(row_number) + " test_case values must be strings");
            }
            destination.push_back(item.get<std::string>());
        }
    }
    return result;
}

std::string serialize_test_case(const domain::LogTestCase& test_case) {
    if (test_case.values.empty()) {
        return {};
    }
    Json value = Json::object();
    for (const auto& [token, values] : test_case.values) {
        value[token] = values;
    }
    return value.dump();
}

void append_field(std::string& output, const std::string_view value) {
    const bool quote = value.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!quote) {
        output.append(value);
        return;
    }
    output.push_back('"');
    for (const char character : value) {
        if (character == '"') {
            output.append("\"\"");
        } else {
            output.push_back(character);
        }
    }
    output.push_back('"');
}

void append_row(std::string& output, const std::array<std::string_view, 5>& values) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output.push_back(',');
        }
        append_field(output, values[index]);
    }
    output.push_back('\n');
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
        const bool retryable = std::ranges::any_of(retryable_errors, [final_error](const DWORD value) {
            return final_error == static_cast<int>(value);
        });
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

std::vector<domain::LogTemplate> CsvLogCatalog::load(const std::filesystem::path& file) const {
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw catalog_error(file, "file was not found or could not be opened");
    }
    std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view utf8_bom{"\xEF\xBB\xBF", 3};
    if (content.starts_with(utf8_bom)) {
        content.erase(0, utf8_bom.size());
    }
    const auto rows = parse_csv(content, file);
    if (rows.empty()) {
        throw catalog_error(file, "a header row is required");
    }
    const auto columns = read_header(rows.front(), file);

    std::vector<domain::LogTemplate> result;
    result.reserve(rows.size() - 1);
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(rows.size() - 1);
    for (std::size_t index = 1; index < rows.size(); ++index) {
        const auto& row = rows[index];
        if (row.size() != rows.front().size()) {
            throw catalog_error(file, "row " + std::to_string(index + 1) + " has " + std::to_string(row.size()) + " fields; expected " + std::to_string(rows.front().size()));
        }
        domain::LogTemplate item;
        item.id = column_value(row, columns, "id");
        item.name = column_value(row, columns, "name");
        item.sample = column_value(row, columns, "sample");
        if (const auto source = columns.find("source"); source != columns.end()) {
            item.source = row[source->second];
        }
        if (const auto test_case = columns.find("test_case"); test_case != columns.end()) {
            item.test_case = parse_test_case(row[test_case->second], file, index + 1);
        }
        if (!domain::valid_sample_id(item.id)) {
            throw catalog_error(file, "row " + std::to_string(index + 1) + " id must contain only digits");
        }
        if (item.name.empty() || item.sample.empty()) {
            throw catalog_error(file, "row " + std::to_string(index + 1) + " name and sample must not be empty");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file, "duplicate id '" + item.id + "'");
        }
        result.push_back(std::move(item));
    }
    return result;
}

void CsvLogCatalog::save(const std::filesystem::path& file, const std::span<const domain::LogTemplate> items) const {
    std::unordered_set<std::string> identifiers;
    identifiers.reserve(items.size());
    std::string content{"id,name,source,sample,test_case\n"};
    for (const auto& item : items) {
        if (!domain::valid_sample_id(item.id)) {
            throw catalog_error(file, "id must contain only digits");
        }
        if (item.name.empty() || item.sample.empty()) {
            throw catalog_error(file, "name and sample must not be empty");
        }
        if (!identifiers.insert(item.id).second) {
            throw catalog_error(file, "duplicate id '" + item.id + "'");
        }
        const auto test_case = serialize_test_case(item.test_case);
        append_row(content, {item.id, item.name, item.source, item.sample, test_case});
    }

    const auto parent = file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const auto temporary = temporary_path_for(file);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw catalog_error(file, "temporary file could not be opened");
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
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
