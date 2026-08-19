// src/application/log_catalog_service.hpp
#pragma once

#include "application/ports/log_catalog.hpp"
#include "application/use_cases/log_catalog.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

class LogCatalogService final : public ILogCatalogUseCase {
public:
    explicit LogCatalogService(ILogCatalog& catalog) noexcept;
    [[nodiscard]] std::vector<domain::LogTemplate> load(const std::filesystem::path& file) const override;
    void save(const std::filesystem::path& file, std::span<const domain::LogTemplate> items) const override;
    [[nodiscard]] std::string next_id(std::span<const domain::LogTemplate> items) const override;
    [[nodiscard]] LogTemplateAnalysis analyze(const domain::LogTemplate& item) const override;
    [[nodiscard]] LogTemplateAnalysis analyze(std::string_view sample) const override;
    [[nodiscard]] std::string sanitize(std::string_view sample) const override;
    [[nodiscard]] std::string privacy_search_terms(const LogTemplateAnalysis& analysis) const override;

private:
    ILogCatalog& catalog_;
};

}
