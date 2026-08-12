// tests/file_transport_tests.cpp
#include "test_support.hpp"

#include "infrastructure/file_transport.hpp"
#include "infrastructure/transport_factory.hpp"

#include <Windows.h>

#include <algorithm>
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
    {
        infrastructure::FileTransport disconnected{directory};
        bool rejected = false;
        try {
            disconnected.send("event");
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        expect(rejected, "FILE transport accepted a write before opening an output file");
    }
    constexpr std::size_t payload_size = 60 * 1024;
    constexpr std::size_t payload_count = 35;
    const std::string payload(payload_size, 'L');
    {
        const infrastructure::TransportFactory factory{directory};
        auto created = factory.create(domain::TransportProtocol::File);
        auto& transport = dynamic_cast<infrastructure::FileTransport&>(*created);
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        transport.connect(endpoint);
        expect(!transport.is_datagram(), "FILE transport must use batched stream writes");
        for (std::size_t index = 0; index < payload_count; ++index) {
            transport.send(payload);
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
    std::filesystem::remove_all(directory, cleanup_error);

    const std::filesystem::path large_directory{directory.wstring() + L"_large"};
    std::filesystem::remove_all(large_directory, cleanup_error);
    {
        infrastructure::FileTransport transport{large_directory};
        domain::EndpointConfig endpoint;
        endpoint.protocol = domain::TransportProtocol::File;
        transport.connect(endpoint);
        transport.send(std::string(infrastructure::FileTransport::slice_size_bytes + 4096, 'X'));
    }
    std::vector<std::uintmax_t> large_sizes;
    for (const auto& entry : std::filesystem::directory_iterator(large_directory)) {
        large_sizes.push_back(entry.file_size());
    }
    std::ranges::sort(large_sizes);
    expect(large_sizes.size() == 2, "A large FILE payload was not split across slices");
    expect(large_sizes.back() == infrastructure::FileTransport::slice_size_bytes, "A generated log slice exceeded 1 MiB");
    std::filesystem::remove_all(large_directory, cleanup_error);
}

}
