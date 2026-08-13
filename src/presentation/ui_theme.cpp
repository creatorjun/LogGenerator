// src/presentation/ui_theme.cpp
#include "presentation/ui_theme.hpp"

#include <imgui.h>

#include <algorithm>

namespace loggen::presentation {

void apply_ui_theme(const float scale) {
    ImGuiStyle style{};
    style.WindowPadding = ImVec2(20.0F, 16.0F);
    style.FramePadding = ImVec2(11.0F, 7.0F);
    style.CellPadding = ImVec2(9.0F, 6.0F);
    style.ItemSpacing = ImVec2(10.0F, 8.0F);
    style.ItemInnerSpacing = ImVec2(7.0F, 5.0F);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 9.0F;
    style.FrameRounding = 6.0F;
    style.PopupRounding = 8.0F;
    style.GrabRounding = 6.0F;
    style.ScrollbarRounding = 8.0F;
    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.ScrollbarSize = 14.0F;
    const float safe_scale = std::clamp(scale, 0.80F, 4.0F);
    style.ScaleAllSizes(safe_scale);
    style.FontScaleDpi = safe_scale;
    ImGui::GetStyle() = style;

    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.10F, 0.13F, 0.18F, 1.0F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.39F, 0.44F, 0.52F, 1.0F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.955F, 0.966F, 0.982F, 1.0F);
    colors[ImGuiCol_ChildBg] = ImVec4(1.0F, 1.0F, 1.0F, 1.0F);
    colors[ImGuiCol_PopupBg] = ImVec4(1.0F, 1.0F, 1.0F, 0.99F);
    colors[ImGuiCol_Border] = ImVec4(0.78F, 0.81F, 0.86F, 1.0F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.925F, 0.94F, 0.965F, 1.0F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.865F, 0.91F, 0.985F, 1.0F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.80F, 0.87F, 0.97F, 1.0F);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_Button] = ImVec4(0.10F, 0.42F, 0.88F, 1.0F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.06F, 0.35F, 0.79F, 1.0F);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.04F, 0.29F, 0.70F, 1.0F);
    colors[ImGuiCol_Header] = ImVec4(0.18F, 0.49F, 0.92F, 0.26F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18F, 0.49F, 0.92F, 0.38F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.18F, 0.49F, 0.92F, 0.52F);
    colors[ImGuiCol_CheckMark] = ImVec4(0.08F, 0.39F, 0.83F, 1.0F);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.12F, 0.45F, 0.88F, 1.0F);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.05F, 0.34F, 0.75F, 1.0F);
    colors[ImGuiCol_Separator] = ImVec4(0.78F, 0.81F, 0.86F, 1.0F);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.92F, 0.94F, 0.97F, 1.0F);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.66F, 0.70F, 0.77F, 1.0F);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55F, 0.61F, 0.70F, 1.0F);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.44F, 0.51F, 0.62F, 1.0F);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.90F, 0.93F, 0.97F, 1.0F);
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_Border];
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.86F, 0.88F, 0.92F, 1.0F);
    colors[ImGuiCol_NavCursor] = ImVec4(0.07F, 0.36F, 0.82F, 1.0F);
}

}
