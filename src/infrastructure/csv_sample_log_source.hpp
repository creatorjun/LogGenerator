// src/infrastructure/csv_sample_log_source.hpp
#pragma once

#include "application/ports/sample_log_source.hpp"

namespace loggen::infrastructure {

class CsvSampleLogSource final : public application::ISampleLogSource {
public:
    [[nodiscard]] std::vector<std::string> load(const std::filesystem::path& file) const override;
};

}
