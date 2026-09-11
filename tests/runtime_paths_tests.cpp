// tests/runtime_paths_tests.cpp
#include "test_support.hpp"

#include "infrastructure/runtime_paths.hpp"

#include <array>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace loggen::tests {
namespace {

class RuntimeFixture final {
public:
    RuntimeFixture() : root(unique_test_path("loggen_runtime_paths_")) {
        std::filesystem::create_directories(root);
    }

    ~RuntimeFixture() {
        std::error_code error;
        for (std::filesystem::recursive_directory_iterator iterator(root, error), end; iterator != end && !error; iterator.increment(error)) {
            std::error_code permissions_error;
            std::filesystem::permissions(iterator->path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, permissions_error);
        }
        std::filesystem::remove_all(root, error);
    }

    RuntimeFixture(const RuntimeFixture&) = delete;
    RuntimeFixture& operator=(const RuntimeFixture&) = delete;

    const std::filesystem::path root;
};

void write_file(const std::filesystem::path& path, const std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
    output.close();
    expect(static_cast<bool>(output), "Unable to write runtime path fixture");
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    expect(static_cast<bool>(input), "Unable to read runtime path fixture");
    return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
}

void portable_paths_preserve_existing_behavior() {
    const RuntimeFixture fixture;
    const auto application_directory = fixture.root / "portable";
    const auto working_directory = fixture.root / "working";
    const auto working_catalog = working_directory / "Sample Logs" / "sample_logs.json";
    write_file(working_catalog, "working catalog");

    auto paths = infrastructure::resolve_runtime_paths(application_directory, working_directory);
    expect(!paths.packaged && paths.data_directory == application_directory, "Portable data must remain beside the executable");
    expect(paths.catalog_file == working_catalog, "Portable catalogs must retain the working-directory fallback");
    expect(paths.log_directory == application_directory / "logs", "Portable log directory changed");
    expect(paths.generated_directory == application_directory / "generated", "Portable FILE directory changed");
    expect(paths.font_directory == application_directory / "fonts", "Fonts must remain executable-relative");
    infrastructure::initialize_runtime_catalog(paths);
    expect(!std::filesystem::exists(application_directory), "Portable initialization must not copy or create catalog files");

    const auto executable_catalog = application_directory / "Sample Logs" / "sample_logs.json";
    write_file(executable_catalog, "portable custom catalog");
    paths = infrastructure::resolve_runtime_paths(application_directory, working_directory);
    expect(paths.catalog_file == executable_catalog, "Executable-relative portable catalog must take precedence");
    infrastructure::initialize_runtime_catalog(paths);
    expect(read_file(executable_catalog) == "portable custom catalog", "Portable user catalog was modified during initialization");
}

void package_paths_seed_writable_data_and_preserve_user_edits() {
    const RuntimeFixture fixture;
    const auto application_directory = fixture.root / "WindowsApps" / "LogGenerator_1.0.0.0_x64";
    const auto working_directory = fixture.root / "unrelated";
    const auto local_state = fixture.root / "Packages" / "LogGenerator_family" / "LocalState";
    const auto bundled_catalog = application_directory / "Sample Logs" / "sample_logs.json";
    write_file(bundled_catalog, "bundled original catalog");
    write_file(working_directory / "Sample Logs" / "sample_logs.json", "unrelated catalog");
    std::filesystem::permissions(bundled_catalog, std::filesystem::perms::owner_write, std::filesystem::perm_options::remove);

    const auto paths = infrastructure::resolve_runtime_paths(application_directory, working_directory, local_state);
    expect(paths.packaged && paths.data_directory == local_state, "MSIX data must use package LocalState");
    expect(paths.catalog_file == local_state / "Sample Logs" / "sample_logs.json", "MSIX user catalog must be writable per-user data");
    expect(paths.bundled_catalog_file == bundled_catalog, "MSIX default catalog must be executable-relative");
    expect(paths.log_directory == local_state / "logs", "MSIX logs must not be written to the installation directory");
    expect(paths.generated_directory == local_state / "generated", "MSIX FILE output must not be written to the installation directory");
    expect(paths.font_directory == application_directory / "fonts", "MSIX fonts must remain bundled resources");
    infrastructure::initialize_runtime_catalog(paths);
    expect(read_file(paths.catalog_file) == "bundled original catalog", "Initial MSIX catalog must come from the bundle");
    write_file(paths.catalog_file, "user edited catalog");
    expect(read_file(bundled_catalog) == "bundled original catalog", "Saving user edits must leave bundled catalog unchanged");

    const auto updated_application_directory = fixture.root / "WindowsApps" / "LogGenerator_2.0.0.0_x64";
    write_file(updated_application_directory / "Sample Logs" / "sample_logs.json", "updated bundled catalog");
    const auto updated_paths = infrastructure::resolve_runtime_paths(updated_application_directory, working_directory, local_state);
    infrastructure::initialize_runtime_catalog(updated_paths);
    expect(read_file(updated_paths.catalog_file) == "user edited catalog", "A package update must preserve the user's saved catalog");
    expect(!std::filesystem::exists(application_directory / "logs"), "Runtime initialization wrote logs beside packaged executable");
    expect(!std::filesystem::exists(application_directory / "generated"), "Runtime initialization wrote FILE output beside packaged executable");
}

void package_initialization_is_atomic_and_preserves_existing_data() {
    const RuntimeFixture fixture;
    const auto application_directory = fixture.root / "package";
    const auto paths = infrastructure::resolve_runtime_paths(application_directory, fixture.root / "cwd", fixture.root / "LocalState");
    const std::string content(256 * 1024, 'x');
    write_file(paths.bundled_catalog_file, content);
    std::array<std::exception_ptr, 8> errors;
    {
        std::array<std::jthread, 8> workers;
        for (std::size_t index = 0; index < workers.size(); ++index) {
            workers[index] = std::jthread([&paths, &errors, index] {
                try {
                    infrastructure::initialize_runtime_catalog(paths);
                } catch (...) {
                    errors[index] = std::current_exception();
                }
            });
        }
    }
    for (const auto& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
    expect(read_file(paths.catalog_file) == content, "Concurrent GUI/CLI initialization must publish a complete catalog");
    std::size_t file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(paths.catalog_file.parent_path())) {
        expect(entry.path() == paths.catalog_file, "Catalog initialization leaked a temporary file");
        ++file_count;
    }
    expect(file_count == 1, "Initial package catalog should have exactly one destination file");
    std::filesystem::remove(paths.bundled_catalog_file);
    infrastructure::initialize_runtime_catalog(paths);
    expect(read_file(paths.catalog_file) == content, "Existing user data must not depend on a bundled replacement");
}

void package_initialization_never_uses_the_working_directory_as_a_seed() {
    const RuntimeFixture fixture;
    const auto working_directory = fixture.root / "cwd";
    write_file(working_directory / "Sample Logs" / "sample_logs.json", "unrelated catalog");
    const auto paths = infrastructure::resolve_runtime_paths(fixture.root / "missing-package", working_directory, fixture.root / "LocalState");
    bool rejected = false;
    try {
        infrastructure::initialize_runtime_catalog(paths);
    } catch (const std::filesystem::filesystem_error&) {
        rejected = true;
    }
    expect(rejected, "MSIX initialization must report a missing bundle");
    expect(!std::filesystem::exists(paths.catalog_file), "A failed copy must not publish an incomplete user catalog");
    expect(std::filesystem::is_empty(paths.catalog_file.parent_path()), "A failed seed must clean up temporary files");
}

}

void run_runtime_paths_tests() {
    portable_paths_preserve_existing_behavior();
    package_paths_seed_writable_data_and_preserve_user_edits();
    package_initialization_is_atomic_and_preserves_existing_data();
    package_initialization_never_uses_the_working_directory_as_a_seed();
}

}
