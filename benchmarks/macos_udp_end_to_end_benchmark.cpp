// benchmarks/macos_udp_end_to_end_benchmark.cpp
#include "application/log_catalog_service.hpp"
#include "application/log_preparation_cache.hpp"
#include "application/ports/logger.hpp"
#include "application/stress_test_service.hpp"
#include "infrastructure/json_log_catalog.hpp"
#include "infrastructure/posix_execution_runtime.hpp"
#include "infrastructure/socket_support.hpp"
#include "infrastructure/transport_factory.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

class NullLogger final : public loggen::application::ILogger {
public:
    void log(loggen::application::LogLevel, std::string_view) noexcept override {
    }
};

struct ReceiverResult {
    std::uint64_t datagrams{0};
    std::uint64_t events{0};
    std::uint64_t bytes{0};
    std::chrono::steady_clock::time_point first{};
    std::chrono::steady_clock::time_point last{};
    std::string error;
};

std::uint16_t bind_receiver(loggen::infrastructure::SocketHandle& receiver) {
    receiver.reset(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (!receiver.valid()) {
        throw std::runtime_error(loggen::infrastructure::socket_error_message("UDP benchmark socket"));
    }
    constexpr int receive_buffer = 4 * 1024 * 1024;
    static_cast<void>(::setsockopt(receiver.get(), SOL_SOCKET, SO_RCVBUF, &receive_buffer, sizeof(receive_buffer)));
    constexpr timeval timeout{0, 100'000};
    if (::setsockopt(receiver.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        throw std::runtime_error(loggen::infrastructure::socket_error_message("UDP benchmark receive timeout"));
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(receiver.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
        throw std::runtime_error(loggen::infrastructure::socket_error_message("UDP benchmark bind"));
    }
    socklen_t address_size = sizeof(address);
    if (::getsockname(receiver.get(), reinterpret_cast<sockaddr*>(&address), &address_size) < 0) {
        throw std::runtime_error(loggen::infrastructure::socket_error_message("UDP benchmark getsockname"));
    }
    return ntohs(address.sin_port);
}

void receive_until_idle(const loggen::infrastructure::NativeSocket socket, ReceiverResult& result, const std::stop_token stop_token) noexcept {
    std::array<char, 65'536> buffer{};
    while (true) {
        const auto received = ::recv(socket, buffer.data(), buffer.size(), 0);
        if (received > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (result.first.time_since_epoch().count() == 0) {
                result.first = now;
            }
            result.last = now;
            ++result.datagrams;
            result.bytes += static_cast<std::uint64_t>(received);
            result.events += static_cast<std::uint64_t>(std::count(buffer.begin(), buffer.begin() + received, '\n'));
            continue;
        }
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (stop_token.stop_requested()) {
                return;
            }
            continue;
        }
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received < 0) {
            result.error = loggen::infrastructure::socket_error_message("UDP benchmark receive");
        }
        return;
    }
}

}

int main(const int argument_count, char** argument_values) {
    try {
        const std::uint64_t target_eps = argument_count > 1 ? std::strtoull(argument_values[1], nullptr, 10) : 3'000'000ULL;
        const auto duration = std::chrono::seconds{argument_count > 2 ? static_cast<long long>(std::strtoll(argument_values[2], nullptr, 10)) : 5LL};
        if (target_eps == 0 || duration <= 0s) {
            throw std::invalid_argument("Usage: LogGeneratorMacosUdpBenchmark [target_eps>0] [duration_seconds>0]");
        }

        loggen::infrastructure::SocketRuntime socket_runtime;
        static_cast<void>(socket_runtime);
        loggen::infrastructure::SocketHandle receiver;
        const auto receiver_port = bind_receiver(receiver);
        ReceiverResult receiver_result;
        std::jthread receiver_thread([&](const std::stop_token stop_token) {
            receive_until_idle(receiver.get(), receiver_result, stop_token);
        });

        NullLogger logger;
        loggen::infrastructure::TransportFactory transport_factory{std::filesystem::temp_directory_path()};
        loggen::infrastructure::PosixExecutionRuntime execution_runtime;
        loggen::application::LogPreparationCache preparation_cache;
        loggen::infrastructure::JsonLogCatalog catalog;
        loggen::application::LogCatalogService catalog_service{catalog, preparation_cache};
        loggen::application::StressTestService service{transport_factory, execution_runtime, preparation_cache, logger};
        loggen::domain::GeneratorConfig config;
        config.endpoint.protocol = loggen::domain::TransportProtocol::Udp;
        config.endpoint.host = "127.0.0.1";
        config.endpoint.port = receiver_port;
        config.endpoint.udp_packetization = loggen::domain::UdpPacketization::NewlinePacked;
        config.transmission_mode = loggen::domain::TransmissionMode::Parallel;
        config.target_eps = target_eps;
        config.templates = catalog_service.load(std::filesystem::path{LOGGEN_SOURCE_DIR} / "Sample Logs" / "sample_logs.json");
        if (config.templates.empty()) {
            throw std::runtime_error("UDP benchmark catalog is empty");
        }

        service.start(std::move(config));
        const auto running_deadline = std::chrono::steady_clock::now() + 5s;
        while (service.snapshot().state != loggen::domain::GeneratorState::Running && std::chrono::steady_clock::now() < running_deadline) {
            std::this_thread::sleep_for(1ms);
        }
        if (service.snapshot().state != loggen::domain::GeneratorState::Running) {
            throw std::runtime_error("UDP benchmark sender did not reach RUNNING state");
        }
        std::this_thread::sleep_for(duration);
        service.stop();
        const auto sender = service.snapshot();
        receiver_thread.request_stop();
        receiver_thread.join();
        if (!receiver_result.error.empty()) {
            throw std::runtime_error(receiver_result.error);
        }

        const auto receiver_seconds = receiver_result.first.time_since_epoch().count() == 0
            ? 0.0
            : std::chrono::duration<double>(receiver_result.last - receiver_result.first).count();
        const auto receiver_eps = receiver_seconds <= 0.0 ? 0.0 : static_cast<double>(receiver_result.events) / receiver_seconds;
        const auto receiver_dps = receiver_seconds <= 0.0 ? 0.0 : static_cast<double>(receiver_result.datagrams) / receiver_seconds;
        const auto delivery_ratio = sender.total_messages == 0 ? 0.0 : static_cast<double>(receiver_result.events) / static_cast<double>(sender.total_messages);
        std::cout
            << std::format("target_eps={} duration_seconds={}\n", target_eps, duration.count())
            << std::format("sender_events={} sender_datagrams={} sender_average_eps={:.2f} sender_average_dps={:.2f}\n", sender.total_messages, sender.total_datagrams, sender.average_eps, sender.average_datagrams_per_second)
            << std::format("receiver_events={} receiver_datagrams={} receiver_bytes={} receiver_eps={:.2f} receiver_dps={:.2f}\n", receiver_result.events, receiver_result.datagrams, receiver_result.bytes, receiver_eps, receiver_dps)
            << std::format("delivery_ratio={:.8f} goal_3m={}\n", delivery_ratio, receiver_eps >= 3'000'000.0 ? "PASS" : "FAIL");
        return receiver_eps >= 3'000'000.0 && delivery_ratio >= 0.999 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failure: " << error.what() << '\n';
        return 1;
    }
}
