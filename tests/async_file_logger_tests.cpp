// tests/async_file_logger_tests.cpp
#include "test_support.hpp"

#include "infrastructure/async_file_logger.hpp"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace loggen::tests {

void run_async_file_logger_tests() {
    const auto directory = std::filesystem::current_path() / (".test_file_logger_" + std::to_string(GetCurrentProcessId()));
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);

    {
        infrastructure::AsyncFileLogger logger{directory, "TestLog", 32};
        logger.info("application started");
        logger.warning("line one\nline two");
        logger.error("sample failure");
    }

    std::filesystem::path log_file;
    std::size_t file_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            log_file = entry.path();
            ++file_count;
        }
    }
    expect(file_count == 1, "Expected one daily application log file");

    std::string contents;
    {
        std::ifstream input(log_file, std::ios::binary);
        contents.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    }
    expect(contents.find("[INFO]") != std::string::npos, "Info log level is missing");
    expect(contents.find("[WARN]") != std::string::npos, "Warning log level is missing");
    expect(contents.find("[ERROR]") != std::string::npos, "Error log level is missing");
    expect(contents.find("application started") != std::string::npos, "Info log message is missing");
    expect(contents.find("line one\\nline two") != std::string::npos, "Multiline log sanitization failed");
    expect(contents.find("sample failure") != std::string::npos, "Error log message is missing");

    std::filesystem::remove_all(directory, cleanup_error);
    expect(!cleanup_error, "Application logger test directory cleanup failed");
}

}
