// src/infrastructure/file_log_catalog.hpp
#pragma once

#include "application/ports/log_catalog.hpp"
#include "infrastructure/csv_log_catalog.hpp"
#include "infrastructure/json_log_catalog.hpp"

namespace loggen::infrastructure {

class FileLogCatalog final : public application::ILogCatalog {
public:
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path& file) const override;
    void save(const std::filesystem::path& file, std::span<const domain::LogTemplate> items) const override;

private:
    JsonLogCatalog json_catalog_;
    CsvLogCatalog csv_catalog_;
};

}
