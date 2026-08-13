// tests/windows_icon_tests.cpp
#include "test_support.hpp"

#include "presentation/windows_resource.hpp"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <string>

namespace loggen::tests {
namespace {

class ModuleHandle final {
public:
    explicit ModuleHandle(const HMODULE value) noexcept
        : value_(value) {
    }

    ~ModuleHandle() {
        if (value_ != nullptr) {
            FreeLibrary(value_);
        }
    }

    ModuleHandle(const ModuleHandle&) = delete;
    ModuleHandle& operator=(const ModuleHandle&) = delete;

    [[nodiscard]] HMODULE get() const noexcept {
        return value_;
    }

private:
    HMODULE value_{nullptr};
};

class IconHandle final {
public:
    explicit IconHandle(const HICON value) noexcept
        : value_(value) {
    }

    ~IconHandle() {
        if (value_ != nullptr) {
            DestroyIcon(value_);
        }
    }

    IconHandle(const IconHandle&) = delete;
    IconHandle& operator=(const IconHandle&) = delete;

    [[nodiscard]] HICON get() const noexcept {
        return value_;
    }

private:
    HICON value_{nullptr};
};

}

void run_windows_icon_tests() {
    std::array<wchar_t, 32'768> module_path{};
    const auto length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    expect(length > 0 && length < static_cast<DWORD>(module_path.size()), "Test executable path could not be resolved");
    const auto application_path = std::filesystem::path(std::wstring(module_path.data(), length)).parent_path() / L"LogGenerator.exe";
    const ModuleHandle application{LoadLibraryExW(application_path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)};
    expect(application.get() != nullptr, "LogGenerator executable could not be loaded as a resource image");

    constexpr std::array icon_sizes{16, 32, 256};
    for (const int size : icon_sizes) {
        const IconHandle icon{static_cast<HICON>(LoadImageW(application.get(), MAKEINTRESOURCEW(IDI_LOGGENERATOR), IMAGE_ICON, size, size, LR_DEFAULTCOLOR))};
        expect(icon.get() != nullptr, "LogGenerator icon resource could not be loaded at a required size");
    }
}

}
