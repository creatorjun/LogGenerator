// tests/windows_icon_tests.cpp
#include "test_support.hpp"

#include "loggen_version.h"
#include "presentation/windows_resource.hpp"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

void verify_manifest(const HMODULE application, const bool gui) {
    const auto resource = FindResourceW(application, CREATEPROCESS_MANIFEST_RESOURCE_ID, RT_MANIFEST);
    expect(resource != nullptr, "Application execution manifest must be embedded in the executable");
    const auto resource_size = SizeofResource(application, resource);
    expect(resource_size > 0, "Application execution manifest must not be empty");
    const auto loaded_resource = LoadResource(application, resource);
    expect(loaded_resource != nullptr, "Application execution manifest could not be loaded");
    const auto data = static_cast<const char*>(LockResource(loaded_resource));
    expect(data != nullptr, "Application execution manifest could not be read");
    const std::string_view manifest{data, resource_size};
    expect(manifest.find("requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"") != std::string_view::npos,
           "Application must run with the invoking user's privileges without UI access elevation");
    expect(manifest.find("8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a") != std::string_view::npos,
           "Application must declare Windows 10 compatibility");
    if (gui) {
        expect(manifest.find("PerMonitorV2,PerMonitor") != std::string_view::npos,
               "Desktop manifest must retain per-monitor DPI awareness");
    }
}

void verify_version(const std::filesystem::path& application_path) {
    DWORD unused_handle = 0;
    const auto size = GetFileVersionInfoSizeW(application_path.c_str(), &unused_handle);
    expect(size > 0, "Application must contain readable version information");
    std::vector<unsigned char> information(size);
    expect(GetFileVersionInfoW(application_path.c_str(), 0, size, information.data()) != FALSE,
           "Application version information could not be loaded");

    void* fixed_value = nullptr;
    UINT fixed_size = 0;
    expect(VerQueryValueW(information.data(), L"\\", &fixed_value, &fixed_size) != FALSE,
           "Application fixed version information is missing");
    expect(fixed_value != nullptr && fixed_size >= sizeof(VS_FIXEDFILEINFO), "Application fixed version information is invalid");
    const auto& fixed = *static_cast<const VS_FIXEDFILEINFO*>(fixed_value);
    expect(fixed.dwSignature == VS_FFI_SIGNATURE, "Application version information has an invalid signature");
    expect(HIWORD(fixed.dwFileVersionMS) == LOGGEN_VERSION_MAJOR && LOWORD(fixed.dwFileVersionMS) == LOGGEN_VERSION_MINOR &&
               HIWORD(fixed.dwFileVersionLS) == LOGGEN_VERSION_PATCH && LOWORD(fixed.dwFileVersionLS) == LOGGEN_VERSION_REVISION,
           "Application file version must match the project version");
    expect(fixed.dwProductVersionMS == fixed.dwFileVersionMS && fixed.dwProductVersionLS == fixed.dwFileVersionLS,
           "Application product and file versions must agree");

    void* filename_value = nullptr;
    UINT filename_size = 0;
    expect(VerQueryValueW(information.data(), L"\\StringFileInfo\\040904B0\\OriginalFilename", &filename_value, &filename_size) != FALSE,
           "Application original filename metadata is missing");
    expect(filename_value != nullptr && filename_size > 1, "Application original filename metadata is empty");
    const std::wstring_view filename{static_cast<const wchar_t*>(filename_value), filename_size - 1};
    expect(filename == application_path.filename().wstring(), "Application original filename metadata must match the executable");
}

void verify_application_resources(const std::filesystem::path& application_path, const bool gui) {
    const ModuleHandle application{LoadLibraryExW(application_path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)};
    expect(application.get() != nullptr, "LogGenerator executable could not be loaded as a resource image");

    constexpr std::array icon_sizes{16, 32, 256};
    for (const int size : icon_sizes) {
        const IconHandle icon{static_cast<HICON>(LoadImageW(application.get(), MAKEINTRESOURCEW(IDI_LOGGENERATOR), IMAGE_ICON, size, size, LR_DEFAULTCOLOR))};
        expect(icon.get() != nullptr, "LogGenerator icon resource could not be loaded at a required size");
    }
    verify_manifest(application.get(), gui);
    verify_version(application_path);
}

}

void run_windows_icon_tests() {
    std::array<wchar_t, 32'768> module_path{};
    const auto length = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    expect(length > 0 && length < static_cast<DWORD>(module_path.size()), "Test executable path could not be resolved");
    const auto directory = std::filesystem::path(std::wstring(module_path.data(), length)).parent_path();
    verify_application_resources(directory / L"LogGenerator.exe", true);
    verify_application_resources(directory / L"LogGeneratorCli.exe", false);
}

}
