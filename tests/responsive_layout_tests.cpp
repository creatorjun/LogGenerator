// tests/responsive_layout_tests.cpp
#include "test_support.hpp"

#include "presentation/responsive_layout.hpp"

namespace loggen::tests {

void run_responsive_layout_tests() {
    const auto compact = presentation::responsive_layout(559.0F, 700.0F, 1.0F);
    expect(compact.size == presentation::ResponsiveSize::Compact, "Narrow layouts must use compact mode");
    expect(compact.metric_columns == 1, "Compact metrics must use one column");
    expect(compact.configuration_columns == 1, "Compact configuration must use one column");
    expect(compact.offset_columns == 2, "Compact offsets must wrap into two columns");
    expect(!compact.inline_header_status, "Compact status must wrap below the heading");
    expect(!compact.inline_header_subtitle, "Compact subtitle must wrap below the heading");
    expect(!compact.fill_configuration_height, "Compact configuration panels must remain content sized");

    const auto medium = presentation::responsive_layout(840.0F, 900.0F, 1.5F);
    expect(medium.size == presentation::ResponsiveSize::Medium, "Breakpoints must be DPI independent");
    expect(medium.metric_columns == 2, "Medium metrics must use two columns");
    expect(medium.configuration_columns == 1, "Medium configuration must stack");
    expect(medium.offset_columns == 4, "Medium offsets must use four columns");
    expect(!medium.inline_header_status, "Medium status must use its own row");
    expect(medium.inline_header_subtitle, "Medium subtitle must remain inline");
    expect(!medium.fill_configuration_height, "Stacked medium panels must remain content sized");

    const auto wide = presentation::responsive_layout(1960.0F, 1400.0F, 2.0F);
    expect(wide.size == presentation::ResponsiveSize::Wide, "Wide breakpoint must account for DPI scale");
    expect(wide.metric_columns == 4, "Wide metrics must use four columns");
    expect(wide.configuration_columns == 2, "Wide configuration must use two columns");
    expect(wide.offset_columns == 4, "Wide offsets must use four columns");
    expect(wide.inline_header_status, "Wide status must remain inline");
    expect(wide.inline_header_subtitle, "Wide subtitle must remain inline");
    expect(wide.fill_configuration_height, "Wide configuration panels must fill remaining vertical space");
    expect(!wide.strong_metric_weight, "Short logical viewports must avoid oversized metric typography");

    const auto maximized = presentation::responsive_layout(2500.0F, 1300.0F, 1.10F);
    expect(maximized.size == presentation::ResponsiveSize::Wide, "Maximized high-resolution layouts must remain wide");
    expect(maximized.fill_configuration_height, "Maximized panels must fill the viewport height");
    expect(maximized.strong_metric_weight, "Tall maximized viewports must use stronger metric typography");

    expect(presentation::responsive_visual_scale(1280.0F, 720.0F, 1.0F) == 0.94F, "Low-resolution visual scale is incorrect");
    expect(presentation::responsive_visual_scale(1920.0F, 1080.0F, 1.0F) == 1.04F, "Full-HD visual scale is incorrect");
    expect(presentation::responsive_visual_scale(2560.0F, 1440.0F, 1.0F) == 1.10F, "QHD visual scale is incorrect");
    expect(presentation::responsive_visual_scale(3840.0F, 2160.0F, 2.0F) == 1.04F, "DPI-independent 4K visual scale is incorrect");
}

}
