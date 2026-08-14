// tests/file_transport_tests.cpp
#include "test_support.hpp"

#include "infrastructure/file_transport.hpp"
#include "infrastructure/transport_factory.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace loggen::tests {
namespace {

std::vector<std::filesystem::path> generated_files(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    return files;
}

std::string read_file(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}

void run_file_transport_tests() {
    const domain::EndpointConfig default_endpoint;
    expect(default_endpoint.file_max_total_bytes == 0, "Default FILE total byte limit must be unlimited");
    expect(default_endpoint.file_max_count == 0, "Default FILE count limit must be unlimited");
    expect(default_endpoint.file_max_duration.count() == 0, "Default FILE duration limit must be unlimited");
    expect(default_endpoint.file_output_directory.empty(), "Default FILE output directory must use the factory path");

    const auto directory = unique_test_path("loggen_generated_");
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    const std::string batch{"first log\nsecond log\n"};
    const std::string large_log(2 * 1024 * 1024, 'L');
    {
        const infrastructure::TransportFactory factory{directory};
        auto created = factory.create(domain::TransportProtocol::File);
        auto* transport = dynamic_cast<infrastructure::FileTransport*>(created.get());
        expect(transport != nullptr, "FILE protocol created a network transport");
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        transport->connect(endpoint);
        expect(generated_files(directory).empty(), "FILE transport created an empty file before receiving a log");
        expect(!transport->is_datagram(), "FILE transport must use ordered stream writes");
        expect(transport->send(batch) == application::SendResult::Sent, "FILE transport rejected a valid log batch");
        expect(transport->send(large_log) == application::SendResult::Sent, "FILE transport rejected a large log event");
    }

    const auto files = generated_files(directory);
    expect(files.size() == 3, "FILE transport did not create exactly one file per log event");
    const std::regex filename_pattern{R"(^\d{8}_\d{6}_\d{3}(?:_\d{4,})?\.log$)", std::regex::ECMAScript};
    for (const auto& file : files) {
        expect(std::regex_match(file.filename().string(), filename_pattern), "Generated log filename is not timestamp based");
    }
    expect(read_file(files[0]) == "first log\n", "First generated file does not contain exactly one log");
    expect(read_file(files[1]) == "second log\n", "Second generated file does not contain exactly one log");
    expect(read_file(files[2]) == large_log, "A log larger than 1 MiB was split or changed");
    expect(domain::protocol_name(domain::TransportProtocol::File) == "FILE", "FILE protocol name is incorrect");

    const auto batch_directory = directory / "batch";
    std::vector<std::string> expected_logs;
    std::string multi_log_batch;
    for (std::size_t index = 0; index < 256; ++index) {
        expected_logs.push_back("batch-log-" + std::to_string(index) + "\n");
        multi_log_batch.append(expected_logs.back());
    }
    {
        infrastructure::FileTransport transport{batch_directory};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        transport.connect(endpoint);
        expect(transport.send(multi_log_batch) == application::SendResult::Sent, "FILE transport rejected a multi-log batch");
    }
    const auto batch_files = generated_files(batch_directory);
    expect(batch_files.size() == expected_logs.size(), "FILE batch created an unexpected number of files");
    for (std::size_t index = 0; index < batch_files.size(); ++index) {
        expect(read_file(batch_files[index]) == expected_logs[index], "FILE batch changed log ordering or content");
    }

    const auto limit_directory = directory / "limits";
    {
        infrastructure::FileTransport transport{limit_directory / "default-output"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_output_directory = (limit_directory / "custom-output").string();
        transport.connect(endpoint);
        expect(transport.send("custom\n") == application::SendResult::Sent, "FILE custom output directory rejected a log");
        expect(generated_files(limit_directory / "custom-output").size() == 1, "FILE custom output directory was not applied");
        expect(!std::filesystem::exists(limit_directory / "default-output"), "FILE factory output directory was used despite an override");
    }
    {
        infrastructure::FileTransport transport{limit_directory / "bytes"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 10;
        endpoint.file_max_count = 0;
        endpoint.file_max_duration = std::chrono::milliseconds{0};
        transport.connect(endpoint);
        expect(transport.send("12345\n") == application::SendResult::Sent, "FILE total byte limit rejected a fitting log");
        expect(transport.send("67890\n") == application::SendResult::TotalBytesLimitReached, "FILE total byte limit was not enforced");
    }
    {
        infrastructure::FileTransport transport{limit_directory / "files"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 0;
        endpoint.file_max_count = 1;
        endpoint.file_max_duration = std::chrono::milliseconds{0};
        transport.connect(endpoint);
        expect(transport.send("one\ntwo\n") == application::SendResult::FileCountLimitReached, "FILE count limit accepted too many log files");
        expect(generated_files(limit_directory / "files").empty(), "FILE count limit partially wrote a rejected batch");
        expect(transport.send("one\n") == application::SendResult::Sent, "FILE count limit rejected the allowed log file");
        expect(transport.send("two\n") == application::SendResult::FileCountLimitReached, "FILE count limit was not enforced per log file");
        expect(generated_files(limit_directory / "files").size() == 1, "FILE count limit created an unexpected number of files");
    }
    {
        infrastructure::FileTransport transport{limit_directory / "duration"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 0;
        endpoint.file_max_count = 0;
        endpoint.file_max_duration = std::chrono::milliseconds{1};
        transport.connect(endpoint);
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        expect(transport.send("delayed\n") == application::SendResult::DurationLimitReached, "FILE duration limit was not enforced");
    }
    std::filesystem::remove_all(directory, cleanup_error);
}

}
