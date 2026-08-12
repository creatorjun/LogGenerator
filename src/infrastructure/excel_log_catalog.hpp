// src/infrastructure/excel_log_catalog.hpp
#pragma once

#include "application/ports/log_catalog.hpp"

namespace loggen::infrastructure {

class ExcelLogCatalog final : public application::ILogCatalog {
public:
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path& directory) const override;
};

}
