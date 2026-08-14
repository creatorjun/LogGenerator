// src/infrastructure/socket_support.hpp
#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <cstdint>
#include <string>
#include <string_view>

namespace loggen::infrastructure {

#ifdef _WIN32
using NativeSocket = SOCKET;
inline constexpr NativeSocket invalid_socket = INVALID_SOCKET;
#else
using NativeSocket = int;
inline constexpr NativeSocket invalid_socket = -1;
#endif

class SocketRuntime final {
public:
    SocketRuntime();
    ~SocketRuntime();

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
};

class SocketHandle {
public:
    SocketHandle() noexcept = default;
    explicit SocketHandle(NativeSocket value) noexcept;
    ~SocketHandle();

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept;
    SocketHandle& operator=(SocketHandle&& other) noexcept;

    [[nodiscard]] NativeSocket get() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    NativeSocket release() noexcept;
    void reset(NativeSocket value = invalid_socket) noexcept;

private:
    NativeSocket value_{invalid_socket};
};

[[nodiscard]] SocketHandle connect_socket(const SocketRuntime& runtime, const std::string& host, std::uint16_t port, int socket_type, int protocol, int timeout_milliseconds = 5000);
void configure_send_buffer(NativeSocket socket, int bytes);
void configure_send_timeout(NativeSocket socket, int milliseconds);
void configure_tcp_stream(NativeSocket socket);
void configure_udp_stream(NativeSocket socket) noexcept;
void send_all(NativeSocket socket, std::string_view payload);
void send_datagram(NativeSocket socket, std::string_view payload);
[[nodiscard]] std::string socket_error_message(std::string_view action, int error_code = -1);

#ifdef _WIN32
[[nodiscard]] std::wstring utf8_to_wide(std::string_view value);
#endif

}
