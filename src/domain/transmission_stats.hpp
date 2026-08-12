// src/domain/transmission_stats.hpp
#pragma once

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
    std::uint64_t total_messages{0};
    std::uint64_t total_bytes{0};
    std::uint64_t send_errors{0};
    double current_eps{0.0};
    double average_eps{0.0};
    double elapsed_seconds{0.0};
    std::string last_error;
};

}
