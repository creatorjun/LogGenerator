#include "test_support.hpp"

#include "domain/generator_config.hpp"
#include "infrastructure/socket_support.hpp"
#include "infrastructure/udp_transport.hpp"

#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace loggen::tests {
namespace {

using infrastructure::NativeSocket;
using infrastructure::SocketHandle;

[[nodiscard]] bool socket_call_failed(const int result) noexcept {
#ifdef _WIN32
    return result == SOCKET_ERROR;
#else
    return result < 0;
#endif
}

[[nodiscard]] SocketHandle bind_loopback_udp_socket() {
    SocketHandle socket_handle{::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)};
    if (!socket_handle.valid()) {
        throw std::runtime_error(infrastructure::socket_error_message("UDP test socket"));
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
#ifdef _WIN32
    const int result = ::bind(socket_handle.get(), reinterpret_cast<const sockaddr*>(&address), static_cast<int>(sizeof(address)));
#else
    const int result = ::bind(socket_handle.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
#endif
    if (socket_call_failed(result)) {
        throw std::runtime_error(infrastructure::socket_error_message("UDP test bind"));
    }
    return socket_handle;
}

[[nodiscard]] std::uint16_t bound_port(const NativeSocket socket) {
    sockaddr_in address{};
#ifdef _WIN32
    int address_length = sizeof(address);
    const int result = ::getsockname(socket, reinterpret_cast<sockaddr*>(&address), &address_length);
#else
    socklen_t address_length = sizeof(address);
    const int result = ::getsockname(socket, reinterpret_cast<sockaddr*>(&address), &address_length);
#endif
    if (socket_call_failed(result)) {
        throw std::runtime_error(infrastructure::socket_error_message("UDP test getsockname"));
    }
    return ntohs(address.sin_port);
}

void configure_receive_timeout(const NativeSocket socket) {
#ifdef _WIN32
    constexpr DWORD timeout_milliseconds = 1'000;
    const int result = ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_milliseconds), sizeof(timeout_milliseconds));
#else
    constexpr timeval timeout{1, 0};
    const int result = ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    if (socket_call_failed(result)) {
        throw std::runtime_error(infrastructure::socket_error_message("UDP test receive timeout"));
    }
}

[[nodiscard]] domain::EndpointConfig loopback_endpoint(const std::uint16_t port) {
    domain::EndpointConfig endpoint;
    endpoint.protocol = domain::TransportProtocol::Udp;
    endpoint.host = "127.0.0.1";
    endpoint.port = port;
    return endpoint;
}

}

void run_udp_transport_tests() {
    infrastructure::SocketRuntime runtime;

    auto receiver = bind_loopback_udp_socket();
    configure_receive_timeout(receiver.get());
    infrastructure::UdpTransport sender{runtime};
    sender.connect(loopback_endpoint(bound_port(receiver.get())));

    constexpr std::string_view payload{"LogGenerator UDP loopback test"};
    expect(sender.send(payload) == application::SendResult::Sent, "UDP transport rejected a loopback datagram");

    std::array<char, 256> receive_buffer{};
#ifdef _WIN32
    const int received = ::recv(receiver.get(), receive_buffer.data(), static_cast<int>(receive_buffer.size()), 0);
#else
    const auto received = ::recv(receiver.get(), receive_buffer.data(), receive_buffer.size(), 0);
#endif
    expect(received == static_cast<decltype(received)>(payload.size()), "UDP loopback receiver got an unexpected payload size");
    expect(std::string_view(receive_buffer.data(), static_cast<std::size_t>(received)) == payload, "UDP loopback receiver got changed payload data");

    auto released_receiver = bind_loopback_udp_socket();
    const auto unused_port = bound_port(released_receiver.get());
    released_receiver.reset();

    infrastructure::UdpTransport unobserved_sender{runtime};
    unobserved_sender.connect(loopback_endpoint(unused_port));
    for (int attempt = 0; attempt < 64; ++attempt) {
        expect(unobserved_sender.send(payload) == application::SendResult::Sent, "An asynchronous UDP port rejection stopped the sender");
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    bool invalid_socket_failed = false;
    try {
        infrastructure::send_datagram(infrastructure::invalid_socket, payload);
    } catch (const std::runtime_error&) {
        invalid_socket_failed = true;
    }
    expect(invalid_socket_failed, "UDP transport suppressed a non-ICMP socket failure");
}

}
