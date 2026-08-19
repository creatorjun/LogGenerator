// src/application/use_cases/stress_test.hpp
#pragma once

#include "domain/generator_config.hpp"
#include "domain/transmission_stats.hpp"

namespace loggen::application {

class IStressTestUseCase {
public:
    virtual ~IStressTestUseCase() = default;

    virtual void start(domain::GeneratorConfig config) = 0;
    virtual void request_stop() noexcept = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual domain::TransmissionStats snapshot() = 0;
};

}
