// src/presentation/windows_icon.cpp
#include "presentation/windows_icon.hpp"

#include "presentation/windows_resource.hpp"

#include <Shobjidl.h>

#include <limits>
#include <string>

namespace loggen::presentation {
namespace {

thread_local HHOOK message_box_hook = nullptr;
thread_local HINSTANCE message_box_instance = nullptr;

std::wstring to_wide(const std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return L"Error message is too long";
    }
    const auto source_size = static_cast<int>(value.size());
    int wide_size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_size, nullptr, 0);
    UINT code_page = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (wide_size == 0) {
        code_page = CP_ACP;
        flags = 0;
        wide_size = MultiByteToWideChar(code_page, flags, value.data(), source_size, nullptr, 0);
    }
    if (wide_size == 0) {
        return L"Unknown application error";
    }
    std::wstring result(static_cast<std::size_t>(wide_size), L'\0');
    if (MultiByteToWideChar(code_page, flags, value.data(), source_size, result.data(), wide_size) == 0) {
        return L"Unknown application error";
    }
    return result;
}

LRESULT CALLBACK message_box_icon_hook(const int code, const WPARAM word_parameter, const LPARAM long_parameter) {
    if (code == HCBT_ACTIVATE && message_box_instance != nullptr) {
        set_application_window_icons(reinterpret_cast<HWND>(word_parameter), message_box_instance);
        const auto hook = message_box_hook;
        message_box_hook = nullptr;
        if (hook != nullptr) {
            UnhookWindowsHookEx(hook);
        }
    }
    return CallNextHookEx(nullptr, code, word_parameter, long_parameter);
}

}

HICON load_application_icon(const HINSTANCE instance, const int width, const int height) noexcept {
    return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_LOGGENERATOR), IMAGE_ICON, width, height, LR_DEFAULTCOLOR | LR_SHARED));
}

void set_application_window_icons(const HWND window, const HINSTANCE instance) noexcept {
    if (window == nullptr || instance == nullptr) {
        return;
    }
    const auto large_icon = load_application_icon(instance, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    const auto small_icon = load_application_icon(instance, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    if (large_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
    }
    if (small_icon != nullptr) {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
}

void configure_application_identity() noexcept {
    static_cast<void>(SetCurrentProcessExplicitAppUserModelID(L"LogGenerator.Desktop"));
}

void show_application_error(const HINSTANCE instance, const std::string_view message) noexcept {
    std::wstring wide_message;
    try {
        wide_message = to_wide(message);
    } catch (...) {
        MessageBoxW(nullptr, L"Unknown application error", L"LogGenerator", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return;
    }

    message_box_instance = instance;
    message_box_hook = SetWindowsHookExW(WH_CBT, &message_box_icon_hook, nullptr, GetCurrentThreadId());
    MessageBoxW(nullptr, wide_message.c_str(), L"LogGenerator", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    if (message_box_hook != nullptr) {
        UnhookWindowsHookEx(message_box_hook);
        message_box_hook = nullptr;
    }
    message_box_instance = nullptr;
}

}
