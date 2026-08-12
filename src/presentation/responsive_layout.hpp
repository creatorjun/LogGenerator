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
};

[[nodiscard]] constexpr ResponsiveLayout responsive_layout(const float available_width, const float scale) noexcept {
    const float safe_scale = scale > 0.5F ? scale : 1.0F;
    const float logical_width = available_width / safe_scale;
    if (logical_width >= 980.0F) {
        return {ResponsiveSize::Wide, 4, 2, 4, true, true};
    }
    if (logical_width >= 560.0F) {
        return {ResponsiveSize::Medium, 2, 1, 4, false, true};
    }
    return {ResponsiveSize::Compact, 1, 1, 2, false, false};
}

}
