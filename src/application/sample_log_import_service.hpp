// src/application/sample_log_import_service.hpp
#pragma once

#include "application/ports/sample_log_source.hpp"
#include "application/use_cases/log_catalog.hpp"
#include "application/use_cases/sample_log_import.hpp"

namespace loggen::application {

class SampleLogImportService final : public ISampleLogImportUseCase {
public:
    SampleLogImportService(ISampleLogSource& source, ILogCatalogUseCase& catalog_service) noexcept;

    [[nodiscard]] std::vector<TokenizedLogTemplate> import_file(
        const std::filesystem::path& file,
        std::span<const domain::LogTemplate> existing_items) const override;

private:
    ISampleLogSource& source_;
    ILogCatalogUseCase& catalog_service_;
};

}
