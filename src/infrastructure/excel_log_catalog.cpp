// src/infrastructure/excel_log_catalog.cpp
#include "infrastructure/excel_log_catalog.hpp"

#include <OpenXLSX.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace loggen::infrastructure {
namespace {

std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::string normalize_header(std::string value) {
    std::erase_if(value, [](const unsigned char character) {
        return std::isspace(character) != 0 || character == '_' || character == '-';
    });
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string cell_text(OpenXLSX::XLWorksheet& sheet, const std::uint32_t row, const std::uint16_t column) {
    return sheet.cell(row, column).value().getString();
}

void load_workbook(const std::filesystem::path& path, std::vector<domain::LogTemplate>& output) {
    OpenXLSX::XLDocument document;
    try {
        document.open(path_to_utf8(path));
        const auto sheet_names = document.workbook().worksheetNames();
        for (const auto& sheet_name : sheet_names) {
            auto sheet = document.workbook().worksheet(sheet_name);
            const auto maximum_header_row = std::min<std::uint32_t>(sheet.rowCount(), 20);
            const auto maximum_header_column = std::min<std::uint16_t>(sheet.columnCount(), 64);
            std::uint32_t header_row = 0;
            std::uint16_t name_column = 0;
            std::uint16_t log_column = 0;
            for (std::uint32_t row = 1; row <= maximum_header_row && log_column == 0; ++row) {
                for (std::uint16_t column = 1; column <= maximum_header_column; ++column) {
                    const auto header = normalize_header(cell_text(sheet, row, column));
                    if (header == "logparsername" || header == "parsername" || header == "name") {
                        name_column = column;
                    } else if (header == "samplelog" || header == "logsample" || header == "sample") {
                        log_column = column;
                    }
                }
                if (name_column != 0 && log_column != 0) {
                    header_row = row;
                    break;
                }
                name_column = 0;
                log_column = 0;
            }
            if (header_row == 0) {
                continue;
            }
            for (std::uint32_t row = header_row + 1; row <= sheet.rowCount(); ++row) {
                auto sample = cell_text(sheet, row, log_column);
                if (sample.empty()) {
                    continue;
                }
                auto name = cell_text(sheet, row, name_column);
                if (name.empty()) {
                    name = path.stem().string() + "-" + std::to_string(row);
                }
                const auto source = path.filename().string() + " / " + sheet_name;
                output.push_back(domain::LogTemplate{source + ':' + std::to_string(row), std::move(name), std::move(sample), source});
            }
        }
        document.close();
    } catch (const std::exception& error) {
        if (document.isOpen()) {
            document.close();
        }
        throw std::runtime_error("Failed to read " + path.string() + ": " + error.what());
    }
}

}

std::vector<domain::LogTemplate> ExcelLogCatalog::load(const std::filesystem::path& directory) const {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Sample Logs directory was not found: " + directory.string());
    }
    std::vector<std::filesystem::path> workbooks;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto filename = entry.path().filename().string();
        if (filename.starts_with("~$")) {
            continue;
        }
        if (lowercase(entry.path().extension().string()) == ".xlsx") {
            workbooks.push_back(entry.path());
        }
    }
    std::ranges::sort(workbooks);
    if (workbooks.empty()) {
        throw std::runtime_error("No .xlsx files were found in " + directory.string());
    }
    std::vector<domain::LogTemplate> result;
    for (const auto& workbook : workbooks) {
        load_workbook(workbook, result);
    }
    if (result.empty()) {
        throw std::runtime_error("No worksheet contains Log Parser Name and Sample Log columns");
    }
    return result;
}

}
