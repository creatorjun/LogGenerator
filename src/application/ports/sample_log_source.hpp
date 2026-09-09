// src/application/ports/sample_log_source.hpp
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace loggen::application {

class ISampleLogSource {
public:
    virtual ~ISampleLogSource() = default;

    [[nodiscard]] virtual std::vector<std::string> load(const std::filesystem::path& file) const = 0;
};

}
