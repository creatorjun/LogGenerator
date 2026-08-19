// src/domain/transmission_stats.hpp
#pragma once

#include "domain/protocol.hpp"

#include <cstdint>
#include <string>

namespace loggen::domain {

enum class GeneratorState {
    Stopped,
    Connecting,
    Running,
    Stopping,
    Failed
};

struct TransmissionStats {
    GeneratorState state{GeneratorState::Stopped};
    TransmissionMode transmission_mode{TransmissionMode::Parallel};
    UdpPacketization udp_packetization{UdpPacketization::OneEventPerDatagram};
    std::uint32_t active_workers{0};
    std::uint64_t total_messages{0};
    std::uint64_t total_datagrams{0};
    std::uint64_t total_bytes{0};
    std::uint64_t send_errors{0};
    double current_eps{0.0};
    double average_eps{0.0};
    double average_datagrams_per_second{0.0};
    double elapsed_seconds{0.0};
    std::string last_error;
    std::string status_message;
};

}
