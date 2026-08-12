// tests/architecture_tests.cpp
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace loggen::tests {
namespace {

std::string read_file(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

bool contains_any(const std::string_view text, const std::vector<std::string_view>& values) {
    for (const auto value : values) {
        if (text.find(value) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

void check_layer(const std::filesystem::path& source_root, const std::string_view layer, const std::vector<std::string_view>& forbidden) {
    const auto directory = source_root / layer;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file() || (entry.path().extension() != ".hpp" && entry.path().extension() != ".cpp")) {
            continue;
        }
        const auto content = read_file(entry.path());
        expect(!contains_any(content, forbidden), "Architecture dependency rule failed for " + entry.path().string());
    }
}

}

void run_architecture_tests() {
    const auto project_root = std::filesystem::path{LOGGEN_SOURCE_DIR};
    const auto source_root = project_root / "src";
    check_layer(source_root, "domain", {"\"application/", "\"infrastructure/", "\"presentation/", "<Windows.h>", "<WinSock2.h>", "<d3d", "<imgui", "<nlohmann/"});
    check_layer(source_root, "application", {"\"infrastructure/", "\"presentation/", "<Windows.h>", "<WinSock2.h>", "<WS2tcpip.h>", "<timeapi.h>", "<d3d", "<dwmapi.h>", "<imgui", "<nlohmann/"});
    check_layer(source_root, "infrastructure", {"\"presentation/"});
    check_layer(source_root, "presentation", {"\"infrastructure/", "\"application/log_catalog_service.hpp\"", "\"application/log_renderer.hpp\"", "\"application/privacy_anonymizer.hpp\"", "\"application/stress_test_service.hpp\""});

    for (const auto directory : {source_root, project_root / "tests", project_root / "benchmarks"}) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file() || (entry.path().extension() != ".hpp" && entry.path().extension() != ".cpp")) {
                continue;
            }
            const auto content = read_file(entry.path());
            const auto end = content.find('\n');
            const auto first_line = content.substr(0, end == std::string::npos ? content.size() : end);
            const auto relative = std::filesystem::relative(entry.path(), project_root).generic_string();
            expect(first_line == "// " + relative, "File path header rule failed for " + relative);
            std::size_t cursor = end == std::string::npos ? content.size() : end + 1;
            while (cursor < content.size()) {
                const auto line_end = content.find('\n', cursor);
                const auto line = std::string_view{content}.substr(cursor, line_end == std::string::npos ? content.size() - cursor : line_end - cursor);
                const auto first_text = line.find_first_not_of(" \t\r");
                if (first_text != std::string_view::npos) {
                    const auto text = line.substr(first_text);
                    expect(!text.starts_with("//") && !text.starts_with("/*") && !text.starts_with('*'), "Additional code comment found in " + relative);
                }
                cursor = line_end == std::string::npos ? content.size() : line_end + 1;
            }
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator(project_root / "scripts")) {
        if (!entry.is_regular_file() || entry.path().extension() != ".ps1") {
            continue;
        }
        const auto content = read_file(entry.path());
        const auto end = content.find('\n');
        auto first_line = content.substr(0, end == std::string::npos ? content.size() : end);
        if (!first_line.empty() && first_line.back() == '\r') {
            first_line.pop_back();
        }
        const auto relative = std::filesystem::relative(entry.path(), project_root).generic_string();
        expect(first_line == "# " + relative, "Script path header rule failed for " + relative);
        std::size_t cursor = end == std::string::npos ? content.size() : end + 1;
        while (cursor < content.size()) {
            const auto line_end = content.find('\n', cursor);
            const auto line = std::string_view{content}.substr(cursor, line_end == std::string::npos ? content.size() - cursor : line_end - cursor);
            const auto first_text = line.find_first_not_of(" \t\r");
            if (first_text != std::string_view::npos) {
                expect(line[first_text] != '#', "Additional script comment found in " + relative);
            }
            cursor = line_end == std::string::npos ? content.size() : line_end + 1;
        }
    }

    const auto cmake = read_file(project_root / "CMakeLists.txt");
    expect(cmake.starts_with("# CMakeLists.txt\n") || cmake.starts_with("# CMakeLists.txt\r\n"), "CMake path header rule failed");
    expect(cmake.find("add_library(loggen_domain INTERFACE)") != std::string::npos, "Domain build boundary is missing");
    expect(cmake.find("add_library(loggen_application STATIC") != std::string::npos, "Application build boundary is missing");
    expect(cmake.find("add_library(loggen_infrastructure STATIC") != std::string::npos, "Infrastructure build boundary is missing");
    expect(cmake.find("loggen_core") == std::string::npos, "Mixed application and infrastructure target remains");
}

}
