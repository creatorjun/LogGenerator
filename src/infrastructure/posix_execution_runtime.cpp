// src/infrastructure/posix_execution_runtime.cpp
#include "infrastructure/posix_execution_runtime.hpp"

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

#include <memory>
#include <algorithm>
#include <cstdint>
#include <thread>

namespace loggen::infrastructure {
namespace {

class PosixExecutionLease final : public application::IExecutionLease {
};

}

std::unique_ptr<application::IExecutionLease> PosixExecutionRuntime::acquire_high_resolution_timer() const {
    return std::make_unique<PosixExecutionLease>();
}

std::uint32_t PosixExecutionRuntime::optimal_worker_count(const domain::TransportProtocol protocol) const noexcept {
    const auto detected = std::max(1U, std::thread::hardware_concurrency());
    if (protocol == domain::TransportProtocol::File) {
        return 1;
    }
    if (protocol == domain::TransportProtocol::Udp) {
#ifdef __APPLE__
        // Connected UDP on macOS converges on the same kernel output path.
        // M4 Max profiling peaks at two senders and regresses beyond that point.
        return std::min(2U, detected);
#else
        return std::min(detected, std::clamp((detected + 3U) / 4U, 2U, 8U));
#endif
    }
    const auto cpu_workers = detected > 2U ? detected - 1U : detected;
    return std::clamp(cpu_workers, 1U, 16U);
}

void PosixExecutionRuntime::configure_current_worker() const noexcept {
#ifdef __APPLE__
    static_cast<void>(pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0));
#endif
}

void PosixExecutionRuntime::pause_current_thread() const noexcept {
    std::this_thread::yield();
}

}
