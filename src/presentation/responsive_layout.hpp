// src/presentation/responsive_layout.hpp
#pragma once

namespace loggen::presentation {

enum class ResponsiveSize {
    Compact,
    Medium,
    Wide
};

struct ResponsiveLayout {
    ResponsiveSize size{ResponsiveSize::Compact};
    int metric_columns{1};
    int configuration_columns{1};
    int offset_columns{2};
    bool inline_header_status{false};
    bool inline_header_subtitle{false};
    bool fill_configuration_height{false};
    bool strong_metric_weight{false};
    float title_scale{1.2F};
    float metric_value_scale{1.15F};
};

[[nodiscard]] constexpr float responsive_visual_scale(const float viewport_width, const float viewport_height, const float dpi_scale) noexcept {
    const float safe_dpi = dpi_scale > 0.5F ? dpi_scale : 1.0F;
    const float logical_width = viewport_width / safe_dpi;
    const float logical_height = viewport_height / safe_dpi;
    if (logical_width >= 2200.0F && logical_height >= 1250.0F) {
        return 1.10F;
    }
    if (logical_width >= 1600.0F && logical_height >= 950.0F) {
        return 1.04F;
    }
    if (logical_width <= 1300.0F || logical_height <= 760.0F) {
        return 0.94F;
    }
    return 1.0F;
}

[[nodiscard]] constexpr ResponsiveLayout responsive_layout(const float available_width, const float available_height, const float scale) noexcept {
    const float safe_scale = scale > 0.5F ? scale : 1.0F;
    const float logical_width = available_width / safe_scale;
    const float logical_height = available_height / safe_scale;
    if (logical_width >= 980.0F) {
        return {ResponsiveSize::Wide, 4, 2, 4, true, true, true, logical_height >= 760.0F, logical_height >= 900.0F ? 1.5F : 1.4F, logical_height >= 900.0F ? 1.4F : 1.28F};
    }
    if (logical_width >= 560.0F) {
        return {ResponsiveSize::Medium, 2, 1, 4, false, true, false, logical_height >= 800.0F, 1.32F, 1.24F};
    }
    return {ResponsiveSize::Compact, 1, 1, 2, false, false, false, false, 1.18F, 1.12F};
}

}
