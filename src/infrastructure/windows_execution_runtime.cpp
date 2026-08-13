// src/infrastructure/windows_execution_runtime.cpp
#include "infrastructure/windows_execution_runtime.hpp"

#include <Windows.h>
#include <timeapi.h>

#include <memory>

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

void WindowsExecutionRuntime::configure_current_worker() const noexcept {
    static_cast<void>(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL));
}

void WindowsExecutionRuntime::pause_current_thread() const noexcept {
    YieldProcessor();
}

}
