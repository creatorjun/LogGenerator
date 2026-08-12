// src/domain/protocol.hpp
#pragma once

#include <string_view>

namespace loggen::domain {

enum class TransportProtocol {
    Udp,
    Tcp,
    Tls
};

enum class StreamFraming {
    Newline,
    OctetCounting
};

constexpr std::string_view protocol_name(const TransportProtocol protocol) noexcept {
    switch (protocol) {
    case TransportProtocol::Udp:
        return "UDP";
    case TransportProtocol::Tcp:
        return "TCP";
    case TransportProtocol::Tls:
        return "TLS";
    }
    return "Unknown";
}

}
