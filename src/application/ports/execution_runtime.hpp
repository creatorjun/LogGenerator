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
    [[nodiscard]] virtual std::unique_ptr<IExecutionLease> acquire_high_resolution_timer() const = 0;
    virtual void configure_current_worker() const noexcept = 0;
    virtual void pause_current_thread() const noexcept = 0;
};

}
