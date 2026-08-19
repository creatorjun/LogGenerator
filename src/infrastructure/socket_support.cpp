// src/infrastructure/socket_support.cpp
#include "infrastructure/socket_support.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <MSWSock.h>
#include <WS2tcpip.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace loggen::infrastructure {
namespace {

int last_socket_error() noexcept {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

void set_last_socket_error(const int error) noexcept {
#ifdef _WIN32
    WSASetLastError(error);
#else
    errno = error;
#endif
}

void close_socket(const NativeSocket socket) noexcept {
#ifdef _WIN32
    static_cast<void>(closesocket(socket));
#else
    static_cast<void>(close(socket));
#endif
}

void set_blocking(const NativeSocket socket, const bool blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0UL : 1UL;
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("ioctlsocket"));
    }
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0 || fcntl(socket, F_SETFL, blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK) < 0) {
        throw std::runtime_error(socket_error_message("fcntl"));
    }
#endif
}

bool wait_for_connect(const NativeSocket socket, const int timeout_milliseconds) {
    fd_set write_set{};
    fd_set error_set{};
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);
    FD_SET(socket, &write_set);
    FD_SET(socket, &error_set);
    timeval timeout{};
    timeout.tv_sec = timeout_milliseconds / 1000;
    timeout.tv_usec = (timeout_milliseconds % 1000) * 1000;
#ifdef _WIN32
    const int result = select(0, nullptr, &write_set, &error_set, &timeout);
#else
    const int result = select(socket + 1, nullptr, &write_set, &error_set, &timeout);
#endif
    if (result <= 0) {
        if (result == 0) {
#ifdef _WIN32
            set_last_socket_error(WSAETIMEDOUT);
#else
            set_last_socket_error(ETIMEDOUT);
#endif
        }
        return false;
    }
    int socket_error = 0;
#ifdef _WIN32
    int length = sizeof(socket_error);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socket_error), &length) == SOCKET_ERROR) {
#else
    socklen_t length = sizeof(socket_error);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &socket_error, &length) < 0) {
#endif
        return false;
    }
    set_last_socket_error(socket_error);
    return socket_error == 0 && FD_ISSET(socket, &write_set);
}

}

SocketRuntime::SocketRuntime() {
#ifdef _WIN32
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        throw std::runtime_error(socket_error_message("WSAStartup", result));
    }
#endif
}

SocketRuntime::~SocketRuntime() {
#ifdef _WIN32
    WSACleanup();
#endif
}

SocketHandle::SocketHandle(const NativeSocket value) noexcept
    : value_(value) {
}

SocketHandle::~SocketHandle() {
    reset();
}

SocketHandle::SocketHandle(SocketHandle&& other) noexcept
    : value_(other.release()) {
}

SocketHandle& SocketHandle::operator=(SocketHandle&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

NativeSocket SocketHandle::get() const noexcept {
    return value_;
}

bool SocketHandle::valid() const noexcept {
    return value_ != invalid_socket;
}

NativeSocket SocketHandle::release() noexcept {
    return std::exchange(value_, invalid_socket);
}

void SocketHandle::reset(const NativeSocket value) noexcept {
    if (valid()) {
#ifdef _WIN32
        static_cast<void>(shutdown(value_, SD_BOTH));
#else
        static_cast<void>(shutdown(value_, SHUT_RDWR));
#endif
        close_socket(value_);
    }
    value_ = value;
}

SocketHandle connect_socket(const SocketRuntime& runtime, const std::string& host, const std::uint16_t port, const int socket_type, const int protocol, const int timeout_milliseconds) {
    static_cast<void>(runtime);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socket_type;
    hints.ai_protocol = protocol;
    addrinfo* raw_results = nullptr;
    const auto service = std::to_string(port);
    const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &raw_results);
    if (lookup != 0) {
#ifdef _WIN32
        throw std::runtime_error("Address resolution failed for " + host + ": " + gai_strerrorA(lookup));
#else
        throw std::runtime_error("Address resolution failed for " + host + ": " + gai_strerror(lookup));
#endif
    }
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> results(raw_results, freeaddrinfo);
#ifdef _WIN32
    int last_error = WSAEHOSTUNREACH;
#else
    int last_error = EHOSTUNREACH;
#endif
    for (auto* address = results.get(); address != nullptr; address = address->ai_next) {
        SocketHandle candidate{socket(address->ai_family, address->ai_socktype, address->ai_protocol)};
        if (!candidate.valid()) {
            last_error = last_socket_error();
            continue;
        }
        try {
            set_blocking(candidate.get(), false);
#ifdef _WIN32
            const int result = ::connect(candidate.get(), address->ai_addr, static_cast<int>(address->ai_addrlen));
            if (result == SOCKET_ERROR) {
                last_error = last_socket_error();
                if (last_error != WSAEWOULDBLOCK && last_error != WSAEINPROGRESS) {
#else
            const int result = ::connect(candidate.get(), address->ai_addr, address->ai_addrlen);
            if (result < 0) {
                last_error = last_socket_error();
                if (last_error != EWOULDBLOCK && last_error != EINPROGRESS) {
#endif
                    continue;
                }
                if (!wait_for_connect(candidate.get(), timeout_milliseconds)) {
                    last_error = last_socket_error();
                    continue;
                }
            }
            set_blocking(candidate.get(), true);
            configure_send_timeout(candidate.get(), timeout_milliseconds);
            return candidate;
        } catch (...) {
            last_error = last_socket_error();
        }
    }
    throw std::runtime_error(socket_error_message("Connection to " + host + ':' + service, last_error));
}

void configure_send_buffer(const NativeSocket socket, const int bytes) {
#ifdef _WIN32
    const int result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bytes), sizeof(bytes));
#else
    const int result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
#endif
    if (result < 0) {
        throw std::runtime_error(socket_error_message("setsockopt SO_SNDBUF"));
    }
}

void configure_send_timeout(const NativeSocket socket, const int milliseconds) {
#ifdef _WIN32
    const int result = setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds));
#else
    const timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
    const int result = setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
    if (result < 0) {
        throw std::runtime_error(socket_error_message("setsockopt SO_SNDTIMEO"));
    }
}

void configure_tcp_stream(const NativeSocket socket) {
    const int enabled = 1;
#ifdef _WIN32
    if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
#else
    if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) < 0) {
#endif
        throw std::runtime_error(socket_error_message("setsockopt TCP_NODELAY"));
    }
#ifdef _WIN32
    static_cast<void>(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&enabled), sizeof(enabled)));
#else
    static_cast<void>(setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)));
#endif
}

void configure_udp_stream(const NativeSocket socket) noexcept {
#ifdef _WIN32
    BOOL disabled = FALSE;
    DWORD returned = 0;
    static_cast<void>(WSAIoctl(socket, SIO_UDP_CONNRESET, &disabled, sizeof(disabled), nullptr, 0, &returned, nullptr, nullptr));
#else
    static_cast<void>(socket);
#endif
}

void send_all(const NativeSocket socket, const std::string_view payload) {
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto remaining = std::min<std::size_t>(payload.size() - offset, static_cast<std::size_t>(std::numeric_limits<int>::max()));
#ifdef _WIN32
        const int sent = ::send(socket, payload.data() + offset, static_cast<int>(remaining), 0);
#else
        const auto sent = ::send(socket, payload.data() + offset, remaining, MSG_NOSIGNAL);
#endif
        if (sent < 0) {
            throw std::runtime_error(socket_error_message("send"));
        }
        if (sent == 0) {
            throw std::runtime_error("Remote endpoint closed the connection");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

void send_datagram(const NativeSocket socket, const std::string_view payload) {
#ifdef _WIN32
    const int sent = ::send(socket, payload.data(), static_cast<int>(payload.size()), 0);
#else
    const auto sent = ::send(socket, payload.data(), payload.size(), MSG_NOSIGNAL);
#endif
    if (sent < 0) {
#ifndef _WIN32
        // A connected POSIX UDP socket reports a delayed ICMP "Port Unreachable"
        // as ECONNREFUSED on a later send. UDP has no delivery acknowledgement,
        // and the Windows path already suppresses the equivalent reset through
        // SIO_UDP_CONNRESET. Keep sender semantics consistent across platforms;
        // permission, routing, buffer and all other socket errors remain fatal.
        if (last_socket_error() == ECONNREFUSED) {
            return;
        }
#endif
        throw std::runtime_error(socket_error_message("UDP send"));
    }
    if (static_cast<std::size_t>(sent) != payload.size()) {
        throw std::runtime_error("UDP send was incomplete");
    }
}

std::string socket_error_message(const std::string_view action, int error_code) {
    if (error_code < 0) {
        error_code = last_socket_error();
    }
    std::string result(action);
    result.append(" failed (");
    result.append(std::to_string(error_code));
    result.push_back(')');
#ifdef _WIN32
    std::array<char, 512> buffer{};
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(error_code), 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    if (length > 0) {
        result.append(": ");
        result.append(buffer.data(), length);
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
            result.pop_back();
        }
    }
#else
    result.append(": ");
    result.append(std::strerror(error_code));
#endif
    return result;
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        throw std::runtime_error("Invalid UTF-8 text");
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), required) <= 0) {
        throw std::runtime_error("UTF-8 conversion failed");
    }
    return result;
}
#endif

}
