// src/infrastructure/runtime_paths.hpp
#pragma once

#include <filesystem>
#include <optional>

namespace loggen::infrastructure {

struct RuntimePaths {
    std::filesystem::path application_directory;
    std::filesystem::path data_directory;
    std::filesystem::path bundled_catalog_file;
    std::filesystem::path catalog_file;
    std::filesystem::path log_directory;
    std::filesystem::path generated_directory;
    std::filesystem::path font_directory;
    bool packaged{false};
};

[[nodiscard]] RuntimePaths resolve_runtime_paths(const std::filesystem::path& application_directory, const std::filesystem::path& working_directory, const std::optional<std::filesystem::path>& package_data_directory = std::nullopt);
[[nodiscard]] RuntimePaths discover_runtime_paths();
void initialize_runtime_catalog(const RuntimePaths& paths);

}
