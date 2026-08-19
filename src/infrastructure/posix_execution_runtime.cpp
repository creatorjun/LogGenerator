// src/infrastructure/posix_execution_runtime.cpp
#include "infrastructure/posix_execution_runtime.hpp"

#ifdef __APPLE__
#include <pthread/qos.h>
#endif

#include <memory>
#include <thread>

namespace loggen::infrastructure {
namespace {

class PosixExecutionLease final : public application::IExecutionLease {
};

}

std::unique_ptr<application::IExecutionLease> PosixExecutionRuntime::acquire_high_resolution_timer() const {
    return std::make_unique<PosixExecutionLease>();
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
