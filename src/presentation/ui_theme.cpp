// src/presentation/ui_theme.cpp
#include "presentation/ui_theme.hpp"

#include <imgui.h>

namespace loggen::presentation {

void apply_ui_theme(const float scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(22.0F, 18.0F);
    style.FramePadding = ImVec2(12.0F, 8.0F);
    style.CellPadding = ImVec2(10.0F, 7.0F);
    style.ItemSpacing = ImVec2(10.0F, 9.0F);
    style.ItemInnerSpacing = ImVec2(8.0F, 6.0F);
    style.WindowRounding = 0.0F;
    style.ChildRounding = 10.0F;
    style.FrameRounding = 7.0F;
    style.PopupRounding = 8.0F;
    style.GrabRounding = 7.0F;
    style.ScrollbarRounding = 9.0F;
    style.WindowBorderSize = 0.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.ScaleAllSizes(scale);

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.91F, 0.93F, 0.96F, 1.0F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.47F, 0.52F, 0.61F, 1.0F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055F, 0.067F, 0.09F, 1.0F);
    colors[ImGuiCol_ChildBg] = ImVec4(0.082F, 0.098F, 0.13F, 1.0F);
    colors[ImGuiCol_PopupBg] = ImVec4(0.075F, 0.09F, 0.12F, 1.0F);
    colors[ImGuiCol_Border] = ImVec4(0.16F, 0.19F, 0.25F, 1.0F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.11F, 0.13F, 0.17F, 1.0F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15F, 0.18F, 0.24F, 1.0F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18F, 0.22F, 0.29F, 1.0F);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_Button] = ImVec4(0.12F, 0.43F, 0.91F, 1.0F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18F, 0.51F, 1.0F, 1.0F);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.10F, 0.36F, 0.78F, 1.0F);
    colors[ImGuiCol_Header] = ImVec4(0.12F, 0.43F, 0.91F, 0.55F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18F, 0.51F, 1.0F, 0.7F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.10F, 0.36F, 0.78F, 0.9F);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33F, 0.68F, 1.0F, 1.0F);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.29F, 0.62F, 1.0F, 1.0F);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.44F, 0.75F, 1.0F, 1.0F);
    colors[ImGuiCol_Separator] = ImVec4(0.16F, 0.19F, 0.25F, 1.0F);
}

}
