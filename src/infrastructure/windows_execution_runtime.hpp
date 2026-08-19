// src/infrastructure/windows_execution_runtime.hpp
#pragma once

#include "application/ports/execution_runtime.hpp"

namespace loggen::infrastructure {

class WindowsExecutionRuntime final : public application::IExecutionRuntime {
public:
    [[nodiscard]] std::unique_ptr<application::IExecutionLease> acquire_high_resolution_timer() const override;
    [[nodiscard]] std::uint32_t optimal_worker_count(domain::TransportProtocol protocol) const noexcept override;
    void configure_current_worker() const noexcept override;
    void pause_current_thread() const noexcept override;
};

}
