// src/application/log_catalog_service.hpp
#pragma once

#include "application/ports/log_catalog.hpp"

#include <filesystem>
#include <span>
#include <vector>

namespace loggen::application {

class LogCatalogService {
public:
    explicit LogCatalogService(ILogCatalog& catalog) noexcept;
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path& file) const;
    void save(const std::filesystem::path& file, std::span<const domain::LogTemplate> items) const;

private:
    ILogCatalog& catalog_;
};

}
