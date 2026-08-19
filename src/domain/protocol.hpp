// src/domain/protocol.hpp
#pragma once

#include <string_view>

namespace loggen::domain {

enum class TransportProtocol {
    Udp,
    Tcp,
    Tls,
    File
};

enum class StreamFraming {
    Newline,
    OctetCounting
};

enum class TransmissionMode {
    Sequential,
    Parallel
};

constexpr std::string_view protocol_name(const TransportProtocol protocol) noexcept {
    switch (protocol) {
    case TransportProtocol::Udp:
        return "UDP";
    case TransportProtocol::Tcp:
        return "TCP";
    case TransportProtocol::Tls:
        return "TLS";
    case TransportProtocol::File:
        return "FILE";
    }
    return "Unknown";
}

constexpr std::string_view transmission_mode_name(const TransmissionMode mode) noexcept {
    switch (mode) {
    case TransmissionMode::Sequential:
        return "sequential";
    case TransmissionMode::Parallel:
        return "parallel-auto";
    }
    return "unknown";
}

}
