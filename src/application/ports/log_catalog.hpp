// src/application/ports/log_catalog.hpp
#pragma once

#include "domain/log_template.hpp"

#include <span>
#include <vector>

namespace loggen::application {

class ILogCatalog {
public:
    virtual ~ILogCatalog() = default;
    [[nodiscard]] virtual std::vector<domain::LogTemplate> load() const = 0;
    virtual void save(std::span<const domain::LogTemplate> items) = 0;
};

}
