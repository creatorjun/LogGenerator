// src/infrastructure/windows_execution_runtime.cpp
#include "infrastructure/windows_execution_runtime.hpp"

#include <Windows.h>
#include <timeapi.h>

#include <memory>

namespace loggen::infrastructure {
namespace {

class TimerResolutionLease final : public application::IExecutionLease {
public:
    TimerResolutionLease()
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

std::unique_ptr<application::IExecutionLease> WindowsExecutionRuntime::enable_high_resolution_timing() const {
    return std::make_unique<TimerResolutionLease>();
}

void WindowsExecutionRuntime::optimize_current_worker() const noexcept {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    THREAD_POWER_THROTTLING_STATE power_state{};
    power_state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
    power_state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
    power_state.StateMask = 0;
    SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &power_state, sizeof(power_state));
}

void WindowsExecutionRuntime::relax_cpu() const noexcept {
    YieldProcessor();
}

}
