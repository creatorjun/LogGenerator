// src/infrastructure/winsock_support.cpp
#include "infrastructure/winsock_support.hpp"

#include <Windows.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace loggen::infrastructure {
namespace {

class WinsockLifetime {
public:
    WinsockLifetime() {
        WSADATA data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::runtime_error(socket_error_message("WSAStartup", result));
        }
    }

    ~WinsockLifetime() {
        WSACleanup();
    }
};

void set_blocking(const SOCKET socket, const bool blocking) {
    u_long mode = blocking ? 0UL : 1UL;
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("ioctlsocket"));
    }
}

bool wait_for_connect(const SOCKET socket, const int timeout_milliseconds, int& socket_error) {
    fd_set write_set{};
    fd_set error_set{};
    FD_ZERO(&write_set);
    FD_ZERO(&error_set);
    FD_SET(socket, &write_set);
    FD_SET(socket, &error_set);
    timeval timeout{};
    timeout.tv_sec = timeout_milliseconds / 1000;
    timeout.tv_usec = (timeout_milliseconds % 1000) * 1000;
    const int result = select(0, nullptr, &write_set, &error_set, &timeout);
    if (result == SOCKET_ERROR) {
        socket_error = WSAGetLastError();
        return false;
    }
    if (result == 0) {
        socket_error = WSAETIMEDOUT;
        return false;
    }
    socket_error = 0;
    int length = sizeof(socket_error);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socket_error), &length) == SOCKET_ERROR) {
        socket_error = WSAGetLastError();
        return false;
    }
    if (socket_error == 0 && !FD_ISSET(socket, &write_set)) {
        socket_error = WSAECONNREFUSED;
        return false;
    }
    return socket_error == 0;
}

}

SocketHandle::SocketHandle(const SOCKET value) noexcept
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

SOCKET SocketHandle::get() const noexcept {
    return value_;
}

bool SocketHandle::valid() const noexcept {
    return value_ != INVALID_SOCKET;
}

SOCKET SocketHandle::release() noexcept {
    return std::exchange(value_, INVALID_SOCKET);
}

void SocketHandle::reset(const SOCKET value) noexcept {
    if (valid()) {
        shutdown(value_, SD_BOTH);
        closesocket(value_);
    }
    value_ = value;
}

void ensure_winsock() {
    static WinsockLifetime lifetime;
    static_cast<void>(lifetime);
}

SocketHandle connect_socket(const std::string& host, const std::uint16_t port, const int socket_type, const int protocol, const int timeout_milliseconds) {
    if (host.empty() || port == 0) {
        throw std::invalid_argument("A destination host and port are required");
    }
    if (timeout_milliseconds <= 0) {
        throw std::invalid_argument("Socket timeout must be greater than zero");
    }
    ensure_winsock();
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socket_type;
    hints.ai_protocol = protocol;
    addrinfo* raw_results = nullptr;
    const auto service = std::to_string(port);
    const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &raw_results);
    if (lookup != 0) {
        const char* description = gai_strerrorA(lookup);
        throw std::runtime_error("Address resolution failed for " + host + ": " + (description == nullptr ? std::to_string(lookup) : std::string(description)));
    }
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> results(raw_results, freeaddrinfo);
    int last_error = WSAEHOSTUNREACH;
    for (auto* address = results.get(); address != nullptr; address = address->ai_next) {
        SocketHandle candidate{socket(address->ai_family, address->ai_socktype, address->ai_protocol)};
        if (!candidate.valid()) {
            last_error = WSAGetLastError();
            continue;
        }
        try {
            set_blocking(candidate.get(), false);
            const int result = ::connect(candidate.get(), address->ai_addr, static_cast<int>(address->ai_addrlen));
            if (result == SOCKET_ERROR) {
                last_error = WSAGetLastError();
                if (last_error != WSAEWOULDBLOCK && last_error != WSAEINPROGRESS) {
                    continue;
                }
                if (!wait_for_connect(candidate.get(), timeout_milliseconds, last_error)) {
                    continue;
                }
            }
            set_blocking(candidate.get(), true);
            configure_send_timeout(candidate.get(), timeout_milliseconds);
            return candidate;
        } catch (...) {
            last_error = WSAGetLastError();
            if (last_error == 0) {
                last_error = WSAEINVAL;
            }
        }
    }
    throw std::runtime_error(socket_error_message("Connection to " + host + ':' + service, last_error));
}

void configure_send_buffer(const SOCKET socket, const int bytes) {
    if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bytes), sizeof(bytes)) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("setsockopt SO_SNDBUF"));
    }
}

void configure_send_timeout(const SOCKET socket, const int milliseconds) {
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds)) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("setsockopt SO_SNDTIMEO"));
    }
}

void configure_receive_timeout(const SOCKET socket, const int milliseconds) {
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds)) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("setsockopt SO_RCVTIMEO"));
    }
}

void send_all(const SOCKET socket, const std::string_view payload) {
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto remaining = std::min<std::size_t>(payload.size() - offset, static_cast<std::size_t>(std::numeric_limits<int>::max()));
        const int sent = ::send(socket, payload.data() + offset, static_cast<int>(remaining), 0);
        if (sent == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
            throw std::runtime_error(socket_error_message("send"));
        }
        if (sent == 0) {
            throw std::runtime_error("Remote endpoint closed the connection");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

void send_all(const SOCKET socket, const std::span<WSABUF> buffers) {
    std::size_t buffer_index = 0;
    while (buffer_index < buffers.size()) {
        while (buffer_index < buffers.size() && buffers[buffer_index].len == 0) {
            ++buffer_index;
        }
        if (buffer_index == buffers.size()) {
            break;
        }
        DWORD sent = 0;
        const auto buffer_count = static_cast<DWORD>(std::min<std::size_t>(buffers.size() - buffer_index, std::numeric_limits<DWORD>::max()));
        const int result = WSASend(socket, buffers.data() + buffer_index, buffer_count, &sent, 0, nullptr, nullptr);
        if (result == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
            throw std::runtime_error(socket_error_message("WSASend"));
        }
        if (sent == 0) {
            throw std::runtime_error("Remote endpoint closed the connection");
        }
        DWORD remaining = sent;
        while (buffer_index < buffers.size() && remaining >= buffers[buffer_index].len) {
            remaining -= buffers[buffer_index].len;
            ++buffer_index;
        }
        if (buffer_index < buffers.size() && remaining > 0) {
            buffers[buffer_index].buf += remaining;
            buffers[buffer_index].len -= remaining;
        }
    }
}

std::string socket_error_message(const std::string_view action, const int error_code) {
    std::array<char, 512> buffer{};
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(error_code), 0, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr);
    std::string result(action);
    result.append(" failed (");
    result.append(std::to_string(error_code));
    result.append(")");
    if (length > 0) {
        result.append(": ");
        result.append(buffer.data(), length);
        while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
            result.pop_back();
        }
    }
    return result;
}

std::wstring utf8_to_wide(const std::string_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("UTF-8 text is too large to convert");
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

}
