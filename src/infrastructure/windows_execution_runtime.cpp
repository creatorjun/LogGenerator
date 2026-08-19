// src/infrastructure/windows_execution_runtime.cpp
#include "infrastructure/windows_execution_runtime.hpp"

#include <Windows.h>
#include <timeapi.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <thread>

namespace loggen::infrastructure {
namespace {

class TimerResolutionLease final : public application::IExecutionLease {
public:
    TimerResolutionLease() noexcept
        : active_(timeBeginPeriod(1) == TIMERR_NOERROR) {
    }

    ~TimerResolutionLease() override {
        if (active_) {
            timeEndPeriod(1);
        }
    }

private:
    bool active_{false};
};

}

std::unique_ptr<application::IExecutionLease> WindowsExecutionRuntime::acquire_high_resolution_timer() const {
    return std::make_unique<TimerResolutionLease>();
}

std::uint32_t WindowsExecutionRuntime::optimal_worker_count(const domain::TransportProtocol protocol) const noexcept {
    const auto detected = std::max(1U, std::thread::hardware_concurrency());
    if (protocol == domain::TransportProtocol::File) {
        return 1;
    }
    if (protocol == domain::TransportProtocol::Udp) {
        return std::min(detected, std::clamp((detected + 3U) / 4U, 2U, 8U));
    }
    const auto cpu_workers = detected > 2U ? detected - 1U : detected;
    return std::clamp(cpu_workers, 1U, 16U);
}

void WindowsExecutionRuntime::configure_current_worker() const noexcept {
    static_cast<void>(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL));
}

void WindowsExecutionRuntime::pause_current_thread() const noexcept {
    YieldProcessor();
}

}
