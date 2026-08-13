// src/infrastructure/winsock_support.hpp
#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace loggen::infrastructure {

class WinsockRuntime final {
public:
    WinsockRuntime();
    ~WinsockRuntime();

    WinsockRuntime(const WinsockRuntime&) = delete;
    WinsockRuntime& operator=(const WinsockRuntime&) = delete;
};

class SocketHandle {
public:
    SocketHandle() noexcept = default;
    explicit SocketHandle(SOCKET value) noexcept;
    ~SocketHandle();

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept;
    SocketHandle& operator=(SocketHandle&& other) noexcept;

    [[nodiscard]] SOCKET get() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    SOCKET release() noexcept;
    void reset(SOCKET value = INVALID_SOCKET) noexcept;

private:
    SOCKET value_{INVALID_SOCKET};
};

[[nodiscard]] SocketHandle connect_socket(const WinsockRuntime& runtime, const std::string& host, std::uint16_t port, int socket_type, int protocol, int timeout_milliseconds = 5000);
void configure_send_buffer(SOCKET socket, int bytes);
void configure_send_timeout(SOCKET socket, int milliseconds);
void send_all(SOCKET socket, std::string_view payload);
[[nodiscard]] std::string socket_error_message(std::string_view action, int error_code = WSAGetLastError());
[[nodiscard]] std::wstring utf8_to_wide(std::string_view value);

}
