// src/infrastructure/csv_sample_log_source.cpp
#include "infrastructure/csv_sample_log_source.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::infrastructure {
namespace {

using CsvRow = std::vector<std::string>;

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::runtime_error csv_error(const std::filesystem::path& file, const std::string& message) {
    return std::runtime_error("CSV 샘플 로그 파일 '" + path_to_utf8(file) + "': " + message);
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
            throw csv_error(file, "NUL 문자는 사용할 수 없습니다.");
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
            throw csv_error(file, "닫는 따옴표 뒤에는 쉼표 또는 줄바꿈만 올 수 있습니다.");
        }
        if (value == '"') {
            if (!field.empty()) {
                throw csv_error(file, "따옴표로 묶은 값은 필드의 첫 문자에서 시작해야 합니다.");
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
        throw csv_error(file, "따옴표로 묶은 필드가 닫히지 않았습니다.");
    }
    finish_row();
    return rows;
}

bool is_optional_header(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return false;
    }
    const auto last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value == "sample" || value == "sample_log" || value == "샘플로그" || value == "샘플 로그";
}

}

std::vector<std::string> CsvSampleLogSource::load(const std::filesystem::path& file) const {
    auto extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (extension != ".csv") {
        throw csv_error(file, "확장자가 .csv인 파일만 가져올 수 있습니다.");
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        throw csv_error(file, "파일을 열 수 없습니다.");
    }
    std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    constexpr std::string_view utf8_bom{"\xEF\xBB\xBF", 3};
    if (content.starts_with(utf8_bom)) {
        content.erase(0, utf8_bom.size());
    }

    const auto rows = parse_csv(content, file);
    if (rows.empty()) {
        throw csv_error(file, "가져올 데이터가 없습니다.");
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index].size() != 1) {
            throw csv_error(file, std::to_string(index + 1) + "행의 컬럼 수가 " + std::to_string(rows[index].size()) + "개입니다. CSV는 정확히 1개 컬럼이어야 합니다.");
        }
    }

    const std::size_t first_sample = is_optional_header(rows.front().front()) ? 1 : 0;
    std::vector<std::string> samples;
    samples.reserve(rows.size() - first_sample);
    for (std::size_t index = first_sample; index < rows.size(); ++index) {
        if (rows[index].front().empty()) {
            throw csv_error(file, std::to_string(index + 1) + "행의 샘플 로그가 비어 있습니다.");
        }
        samples.push_back(rows[index].front());
    }
    if (samples.empty()) {
        throw csv_error(file, "가져올 샘플 로그가 없습니다.");
    }
    return samples;
}

}
