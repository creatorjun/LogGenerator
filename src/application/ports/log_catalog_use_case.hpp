// src/application/ports/log_catalog_use_case.hpp
#pragma once

#include "application/log_template_analysis.hpp"
#include "domain/log_template.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

struct CatalogItem {
    domain::LogTemplate log;
    std::string search_text;
    LogTemplateAnalysis analysis;
};

class ILogCatalogUseCase {
public:
    virtual ~ILogCatalogUseCase() = default;
    [[nodiscard]] virtual std::vector<CatalogItem> load() const = 0;
    virtual void save(std::span<const domain::LogTemplate> items) = 0;
    [[nodiscard]] virtual CatalogItem create(std::span<const domain::LogTemplate> existing, std::string name, std::string sample) = 0;
    [[nodiscard]] virtual CatalogItem update(const domain::LogTemplate& existing, std::string name, std::string sample) const = 0;
    [[nodiscard]] virtual CatalogItem describe(domain::LogTemplate item) const = 0;
    [[nodiscard]] virtual LogTemplateAnalysis analyze(std::string_view sample) const = 0;
};

}
