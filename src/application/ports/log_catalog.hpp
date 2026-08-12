// src/application/ports/log_catalog.hpp
#pragma once

#include "domain/log_template.hpp"

#include <filesystem>
#include <vector>

namespace loggen::application {

class ILogCatalog {
public:
    virtual ~ILogCatalog() = default;
    [[nodiscard]] virtual std::vector<domain::LogTemplate> load(const std::filesystem::path& directory) const = 0;
};

}
