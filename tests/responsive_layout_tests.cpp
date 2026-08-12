// tests/responsive_layout_tests.cpp
#include "test_support.hpp"

#include "presentation/responsive_layout.hpp"

namespace loggen::tests {

void run_responsive_layout_tests() {
    const auto compact = presentation::responsive_layout(559.0F, 1.0F);
    expect(compact.size == presentation::ResponsiveSize::Compact, "Narrow layouts must use compact mode");
    expect(compact.metric_columns == 1, "Compact metrics must use one column");
    expect(compact.configuration_columns == 1, "Compact configuration must use one column");
    expect(compact.offset_columns == 2, "Compact offsets must wrap into two columns");
    expect(!compact.inline_header_status, "Compact status must wrap below the heading");
    expect(!compact.inline_header_subtitle, "Compact subtitle must wrap below the heading");

    const auto medium = presentation::responsive_layout(840.0F, 1.5F);
    expect(medium.size == presentation::ResponsiveSize::Medium, "Breakpoints must be DPI independent");
    expect(medium.metric_columns == 2, "Medium metrics must use two columns");
    expect(medium.configuration_columns == 1, "Medium configuration must stack");
    expect(medium.offset_columns == 4, "Medium offsets must use four columns");
    expect(!medium.inline_header_status, "Medium status must use its own row");
    expect(medium.inline_header_subtitle, "Medium subtitle must remain inline");

    const auto wide = presentation::responsive_layout(1960.0F, 2.0F);
    expect(wide.size == presentation::ResponsiveSize::Wide, "Wide breakpoint must account for DPI scale");
    expect(wide.metric_columns == 4, "Wide metrics must use four columns");
    expect(wide.configuration_columns == 2, "Wide configuration must use two columns");
    expect(wide.offset_columns == 4, "Wide offsets must use four columns");
    expect(wide.inline_header_status, "Wide status must remain inline");
    expect(wide.inline_header_subtitle, "Wide subtitle must remain inline");
}

}
