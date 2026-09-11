// src/infrastructure/runtime_paths.cpp
#include "infrastructure/runtime_paths.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <appmodel.h>
#include <ShlObj.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace loggen::infrastructure {
namespace {

std::filesystem::path executable_directory() {
#ifdef _WIN32
    std::array<wchar_t, 32'768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= static_cast<DWORD>(buffer.size())) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#elif defined(__APPLE__)
    std::uint32_t buffer_size = 1024;
    std::vector<char> buffer(buffer_size);
    if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
        buffer.resize(buffer_size);
        if (_NSGetExecutablePath(buffer.data(), &buffer_size) != 0) {
            return std::filesystem::current_path();
        }
    }
    std::error_code error;
    const auto executable = std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()), error);
    return (error ? std::filesystem::path(buffer.data()) : executable).parent_path();
#else
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::current_path() : executable.parent_path();
#endif
}

std::optional<std::filesystem::path> package_data_directory() {
#ifdef _WIN32
    UINT32 length = 0;
    const LONG size_result = GetCurrentPackageFamilyName(&length, nullptr);
    if (size_result == APPMODEL_ERROR_NO_PACKAGE) {
        return std::nullopt;
    }
    if (size_result != ERROR_INSUFFICIENT_BUFFER || length == 0) {
        throw std::system_error(static_cast<int>(size_result), std::system_category(), "GetCurrentPackageFamilyName");
    }
    std::vector<wchar_t> family_name(length);
    const LONG name_result = GetCurrentPackageFamilyName(&length, family_name.data());
    if (name_result != ERROR_SUCCESS) {
        throw std::system_error(static_cast<int>(name_result), std::system_category(), "GetCurrentPackageFamilyName");
    }
    PWSTR raw_path = nullptr;
    const HRESULT path_result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_NO_PACKAGE_REDIRECTION, nullptr, &raw_path);
    const auto release_path = [](wchar_t* value) noexcept { CoTaskMemFree(value); };
    const std::unique_ptr<wchar_t, decltype(release_path)> local_app_data(raw_path, release_path);
    if (FAILED(path_result) || !local_app_data) {
        throw std::system_error(static_cast<int>(path_result), std::system_category(), "SHGetKnownFolderPath(LocalAppData)");
    }
    return std::filesystem::path(local_app_data.get()) / "Packages" / family_name.data() / "LocalState";
#else
    return std::nullopt;
#endif
}

class TemporaryCatalog final {
public:
    explicit TemporaryCatalog(const std::filesystem::path& destination) : path_(destination) {
        static std::atomic_uint64_t sequence{0};
#ifdef _WIN32
        const auto process_id = static_cast<unsigned long>(GetCurrentProcessId());
#else
        const auto process_id = static_cast<unsigned long>(getpid());
#endif
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ += ".seed." + std::to_string(process_id) + "." + std::to_string(timestamp) + "." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    }

    ~TemporaryCatalog() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryCatalog(const TemporaryCatalog&) = delete;
    TemporaryCatalog& operator=(const TemporaryCatalog&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void publish_catalog(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
    std::error_code error;
#ifdef _WIN32
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
        return;
    }
    error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
    std::filesystem::create_hard_link(temporary, destination, error);
    if (!error) {
        return;
    }
#endif
    std::error_code destination_error;
    if (!std::filesystem::is_regular_file(destination, destination_error)) {
        throw std::filesystem::filesystem_error("Unable to initialize the user sample catalog", temporary, destination, error);
    }
}

}

RuntimePaths resolve_runtime_paths(const std::filesystem::path& application_directory, const std::filesystem::path& working_directory, const std::optional<std::filesystem::path>& package_data_directory) {
    if (application_directory.empty() || (package_data_directory && package_data_directory->empty())) {
        throw std::invalid_argument("Application and package data directories must not be empty");
    }
    RuntimePaths paths;
    paths.application_directory = application_directory;
    paths.packaged = package_data_directory.has_value();
    paths.data_directory = package_data_directory.value_or(application_directory);
    paths.bundled_catalog_file = application_directory / "Sample Logs" / "sample_logs.json";
    paths.catalog_file = paths.packaged ? paths.data_directory / "Sample Logs" / "sample_logs.json" : paths.bundled_catalog_file;
    if (!paths.packaged && !std::filesystem::exists(paths.catalog_file)) {
        paths.catalog_file = working_directory / "Sample Logs" / "sample_logs.json";
    }
    paths.log_directory = paths.data_directory / "logs";
    paths.generated_directory = paths.data_directory / "generated";
    paths.font_directory = application_directory / "fonts";
    return paths;
}

RuntimePaths discover_runtime_paths() {
    const auto application_directory = executable_directory();
    const auto package_directory = package_data_directory();
    return resolve_runtime_paths(application_directory, package_directory ? application_directory : std::filesystem::current_path(), package_directory);
}

void initialize_runtime_catalog(const RuntimePaths& paths) {
    if (!paths.packaged) {
        return;
    }
    if (std::filesystem::exists(paths.catalog_file)) {
        if (!std::filesystem::is_regular_file(paths.catalog_file)) {
            throw std::runtime_error("The user sample catalog path is not a regular file");
        }
        return;
    }
    std::filesystem::create_directories(paths.catalog_file.parent_path());
    const TemporaryCatalog temporary(paths.catalog_file);
    std::filesystem::copy_file(paths.bundled_catalog_file, temporary.path());
    std::filesystem::permissions(temporary.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    publish_catalog(temporary.path(), paths.catalog_file);
}

}
