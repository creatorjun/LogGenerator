// src/application/use_cases/sample_log_import.hpp
#pragma once

#include "application/models/tokenized_log_template.hpp"
#include "domain/log_template.hpp"

#include <filesystem>
#include <span>
#include <vector>

namespace loggen::application {

class ISampleLogImportUseCase {
public:
    virtual ~ISampleLogImportUseCase() = default;

    [[nodiscard]] virtual std::vector<TokenizedLogTemplate> import_file(
        const std::filesystem::path& file,
        std::span<const domain::LogTemplate> existing_items) const = 0;
};

}
