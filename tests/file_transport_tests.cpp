// tests/file_transport_tests.cpp
#include "test_support.hpp"

#include "infrastructure/file_transport.hpp"
#include "infrastructure/transport_factory.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace loggen::tests {

void run_file_transport_tests() {
    const auto directory = std::filesystem::current_path() / (".test_generated_" + std::to_string(GetCurrentProcessId()));
    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    constexpr std::size_t payload_size = 60 * 1024;
    constexpr std::size_t payload_count = 35;
    const std::string payload(payload_size, 'L');
    {
        const infrastructure::TransportFactory factory{directory};
        auto created = factory.create(domain::TransportProtocol::File);
        auto* transport = dynamic_cast<infrastructure::FileTransport*>(created.get());
        expect(transport != nullptr, "FILE protocol created a network transport");
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        transport->connect(endpoint);
        expect(!transport->is_datagram(), "FILE transport must use batched stream writes");
        for (std::size_t index = 0; index < payload_count; ++index) {
            expect(transport->send(payload) == application::SendResult::Sent, "FILE transport stopped before its configured limits");
        }
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
    std::ranges::sort(files);
    expect(files.size() == 3, "FILE transport did not rotate at approximately 1 MiB");
    const std::regex filename_pattern{R"(^\d{8}_\d{6}_\d{3}(?:_\d{4})?\.log$)", std::regex::ECMAScript};
    std::uint64_t total_size = 0;
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto size = std::filesystem::file_size(files[index]);
        total_size += size;
        expect(size <= infrastructure::FileTransport::slice_size_bytes, "Generated log slice exceeded 1 MiB");
        if (index + 1 < files.size()) {
            expect(size >= 900 * 1024, "Generated log slice rotated too early");
        }
        expect(std::regex_match(files[index].filename().string(), filename_pattern), "Generated log filename is not timestamp based");
    }
    expect(total_size == payload_size * payload_count, "FILE transport lost generated log bytes");
    expect(domain::protocol_name(domain::TransportProtocol::File) == "FILE", "FILE protocol name is incorrect");

    const auto limit_directory = directory / "limits";
    {
        infrastructure::FileTransport transport{limit_directory / "bytes"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 100 * 1024;
        endpoint.file_max_count = 0;
        endpoint.file_max_duration = std::chrono::milliseconds{0};
        transport.connect(endpoint);
        expect(transport.send(payload) == application::SendResult::Sent, "FILE total byte limit rejected a fitting payload");
        expect(transport.send(payload) == application::SendResult::TotalBytesLimitReached, "FILE total byte limit was not enforced");
    }
    {
        infrastructure::FileTransport transport{limit_directory / "files"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 0;
        endpoint.file_max_count = 1;
        endpoint.file_max_duration = std::chrono::milliseconds{0};
        transport.connect(endpoint);
        for (std::size_t index = 0; index < 18; ++index) {
            const auto result = transport.send(payload);
            if (index < 18 - 1) {
                expect(result == application::SendResult::Sent, "FILE count limit stopped before the first slice was full");
            } else {
                expect(result == application::SendResult::FileCountLimitReached, "FILE count limit was not enforced");
            }
        }
    }
    {
        infrastructure::FileTransport transport{limit_directory / "duration"};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        endpoint.file_max_total_bytes = 0;
        endpoint.file_max_count = 0;
        endpoint.file_max_duration = std::chrono::milliseconds{1};
        transport.connect(endpoint);
        Sleep(5);
        expect(transport.send(payload) == application::SendResult::DurationLimitReached, "FILE duration limit was not enforced");
    }
    std::filesystem::remove_all(directory, cleanup_error);
}

}
