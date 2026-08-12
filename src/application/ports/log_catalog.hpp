// src/application/ports/log_catalog.hpp
#pragma once

#include "domain/log_template.hpp"

#include <filesystem>
#include <span>
#include <vector>

namespace loggen::application {

class ILogCatalog {
public:
    virtual ~ILogCatalog() = default;
    [[nodiscard]] virtual std::vector<domain::LogTemplate> load(const std::filesystem::path& file) const = 0;
    virtual void save(const std::filesystem::path& file, std::span<const domain::LogTemplate> items) const = 0;
};

}
