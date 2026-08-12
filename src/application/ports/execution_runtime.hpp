// src/application/ports/execution_runtime.hpp
#pragma once

#include <memory>

namespace loggen::application {

class IExecutionLease {
public:
    virtual ~IExecutionLease() = default;
};

class IExecutionRuntime {
public:
    virtual ~IExecutionRuntime() = default;
    [[nodiscard]] virtual std::unique_ptr<IExecutionLease> enable_high_resolution_timing() const = 0;
    virtual void optimize_current_worker() const noexcept = 0;
    virtual void relax_cpu() const noexcept = 0;
};

}
