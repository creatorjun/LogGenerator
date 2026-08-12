// src/infrastructure/windows_execution_runtime.hpp
#pragma once

#include "application/ports/execution_runtime.hpp"

namespace loggen::infrastructure {

class WindowsExecutionRuntime final : public application::IExecutionRuntime {
public:
    [[nodiscard]] std::unique_ptr<application::IExecutionLease> enable_high_resolution_timing() const override;
    void optimize_current_worker() const noexcept override;
    void relax_cpu() const noexcept override;
};

}
