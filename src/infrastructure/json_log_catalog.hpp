// src/infrastructure/json_log_catalog.hpp
#pragma once

#include "application/ports/log_catalog.hpp"

#include <filesystem>

namespace loggen::infrastructure {

class JsonLogCatalog final : public application::ILogCatalog {
public:
    explicit JsonLogCatalog(std::filesystem::path file);
    [[nodiscard]] std::vector<domain::LogTemplate> load() const override;
    void save(std::span<const domain::LogTemplate> items) override;

private:
    std::filesystem::path file_;
};

}
