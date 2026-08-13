// src/presentation/windows_icon.hpp
#pragma once

#include <Windows.h>

#include <string_view>

namespace loggen::presentation {

[[nodiscard]] HICON load_application_icon(HINSTANCE instance, int width, int height) noexcept;
void set_application_window_icons(HWND window, HINSTANCE instance) noexcept;
void configure_application_identity() noexcept;
void show_application_error(HINSTANCE instance, std::string_view message) noexcept;

}
