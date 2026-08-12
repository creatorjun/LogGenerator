// src/application/log_catalog_service.hpp
#pragma once

#include "application/ports/log_catalog.hpp"
#include "application/ports/log_catalog_use_case.hpp"
#include "application/ports/identifier_generator.hpp"
#include "application/ports/logger.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

class LogCatalogService final : public ILogCatalogUseCase {
public:
    LogCatalogService(ILogCatalog& catalog, IIdentifierGenerator& identifier_generator, ILogger& logger);

    [[nodiscard]] std::vector<CatalogItem> load() const override;
    void save(std::span<const domain::LogTemplate> items) override;
    [[nodiscard]] CatalogItem create(std::span<const domain::LogTemplate> existing, std::string name, std::string sample) override;
    [[nodiscard]] CatalogItem update(const domain::LogTemplate& existing, std::string name, std::string sample) const override;
    [[nodiscard]] CatalogItem describe(domain::LogTemplate item) const override;
    [[nodiscard]] LogTemplateAnalysis analyze(std::string_view sample) const override;

private:
    ILogCatalog& catalog_;
    IIdentifierGenerator& identifier_generator_;
    ILogger& logger_;
};

}
