// src/application/use_cases/log_catalog.hpp
#pragma once

#include "application/models/log_template_analysis.hpp"
#include "application/models/tokenized_log_template.hpp"
#include "domain/log_template.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::application {

class ILogCatalogUseCase {
public:
    virtual ~ILogCatalogUseCase() = default;

    [[nodiscard]] virtual std::vector<domain::LogTemplate> load(const std::filesystem::path& file) const = 0;
    virtual void save(const std::filesystem::path& file, std::span<const domain::LogTemplate> items) const = 0;
    [[nodiscard]] virtual std::string next_id(std::span<const domain::LogTemplate> items) const = 0;
    [[nodiscard]] virtual LogTemplateAnalysis analyze(const domain::LogTemplate& item) const = 0;
    [[nodiscard]] virtual LogTemplateAnalysis analyze(std::string_view sample) const = 0;
    [[nodiscard]] virtual std::string sanitize(std::string_view sample) const = 0;
    [[nodiscard]] virtual TokenizedLogTemplate tokenize(domain::LogTemplate item) const = 0;
    [[nodiscard]] virtual std::string privacy_search_terms(const LogTemplateAnalysis& analysis) const = 0;
};

}
