// src/application/stress_test_service.cpp
#include "application/stress_test_service.hpp"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace loggen::application {
namespace {

class RatePacer {
public:
    explicit RatePacer(const std::uint64_t events_per_second)
        : events_per_second_(events_per_second), next_(std::chrono::steady_clock::now()) {
    }

    void wait(const std::uint64_t events, const std::stop_token stop_token) {
        if (events_per_second_ == 0 || events == 0) {
            return;
        }
        const auto duration = std::chrono::nanoseconds{static_cast<std::int64_t>((1'000'000'000ULL * events) / events_per_second_)};
        next_ += duration;
        auto now = std::chrono::steady_clock::now();
        if (next_ + std::chrono::seconds{1} < now) {
            next_ = now;
            return;
        }
        while (!stop_token.stop_requested() && now + std::chrono::microseconds{200} < next_) {
            const auto remaining = next_ - now - std::chrono::microseconds{150};
            const auto maximum_sleep = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds{2});
            std::this_thread::sleep_for(std::min(remaining, maximum_sleep));
            now = std::chrono::steady_clock::now();
        }
        while (!stop_token.stop_requested() && std::chrono::steady_clock::now() < next_) {
            YieldProcessor();
        }
    }

private:
    std::uint64_t events_per_second_{0};
    std::chrono::steady_clock::time_point next_;
};

void append_frame(std::string& batch, const std::string_view payload, const domain::StreamFraming framing) {
    if (framing == domain::StreamFraming::OctetCounting) {
        char size_buffer[32]{};
        const auto conversion = std::to_chars(std::begin(size_buffer), std::end(size_buffer), payload.size());
        batch.append(size_buffer, conversion.ptr);
        batch.push_back(' ');
        batch.append(payload);
        return;
    }
    batch.append(payload);
    if (payload.empty() || (payload.back() != '\n' && payload.back() != '\r')) {
        batch.push_back('\n');
    }
}

std::uint64_t quota_for_worker(const std::uint64_t target_eps, const std::uint32_t worker_index, const std::uint32_t worker_count) {
    if (target_eps == 0) {
        return 0;
    }
    const auto base = target_eps / worker_count;
    return base + (worker_index < target_eps % worker_count ? 1 : 0);
}

std::size_t stream_batch_events(const std::uint64_t quota) {
    if (quota == 0) {
        return 256;
    }
    return static_cast<std::size_t>(std::clamp<std::uint64_t>(quota / 200, 1, 128));
}

}

StressTestService::StressTestService(const ITransportFactory& transport_factory, ILogger& logger)
    : transport_factory_(transport_factory), logger_(logger) {
}

StressTestService::~StressTestService() {
    stop();
}

void StressTestService::start(domain::GeneratorConfig config) {
    stop();
    if (config.templates.empty()) {
        throw std::invalid_argument("At least one log template is required");
    }
    if (config.endpoint.host.empty() || config.endpoint.port == 0) {
        throw std::invalid_argument("A destination host and port are required");
    }
    config.worker_count = std::clamp<std::uint32_t>(config.worker_count, 1, 64);
    if (config.target_eps > 0) {
        config.worker_count = static_cast<std::uint32_t>(std::min<std::uint64_t>(config.worker_count, config.target_eps));
    }
    auto prepared = LogRenderer::prepare(config);
    logger_.info(std::format(
        "Stress test starting: protocol={}, endpoint={}:{}, workers={}, templates={}, target_eps={}",
        domain::protocol_name(config.endpoint.protocol),
        config.endpoint.host,
        config.endpoint.port,
        config.worker_count,
        config.templates.size(),
        config.target_eps == 0 ? std::string("unlimited") : std::to_string(config.target_eps)));

    std::scoped_lock lock(lifecycle_mutex_);
    total_messages_.store(0, std::memory_order_relaxed);
    total_bytes_.store(0, std::memory_order_relaxed);
    send_errors_.store(0, std::memory_order_relaxed);
    connected_workers_.store(0, std::memory_order_relaxed);
    {
        std::scoped_lock error_lock(error_mutex_);
        last_error_.clear();
    }
    stop_source_ = std::stop_source{};
    started_at_ = std::chrono::steady_clock::now();
    meter_at_ = started_at_;
    meter_messages_ = 0;
    current_eps_ = 0.0;
    state_.store(domain::GeneratorState::Connecting, std::memory_order_release);
    workers_.reserve(config.worker_count);
    for (std::uint32_t index = 0; index < config.worker_count; ++index) {
        workers_.emplace_back([this, config, prepared, index](std::stop_token) mutable {
            run_worker(config, std::move(prepared), index, config.worker_count, stop_source_.get_token());
        });
    }
}

void StressTestService::stop() noexcept {
    std::scoped_lock lock(lifecycle_mutex_);
    if (workers_.empty()) {
        state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
        current_eps_ = 0.0;
        return;
    }
    if (state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
        state_.store(domain::GeneratorState::Stopping, std::memory_order_release);
    }
    stop_source_.request_stop();
    workers_.clear();
    state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
    current_eps_ = 0.0;
    logger_.info(std::format(
        "Stress test stopped: messages={}, bytes={}, errors={}",
        total_messages_.load(std::memory_order_relaxed),
        total_bytes_.load(std::memory_order_relaxed),
        send_errors_.load(std::memory_order_relaxed)));
}

domain::TransmissionStats StressTestService::snapshot() {
    domain::TransmissionStats result;
    result.state = state_.load(std::memory_order_acquire);
    result.total_messages = total_messages_.load(std::memory_order_relaxed);
    result.total_bytes = total_bytes_.load(std::memory_order_relaxed);
    result.send_errors = send_errors_.load(std::memory_order_relaxed);
    const auto now = std::chrono::steady_clock::now();
    if (started_at_.time_since_epoch().count() != 0) {
        result.elapsed_seconds = std::chrono::duration<double>(now - started_at_).count();
    }
    {
        std::scoped_lock lock(meter_mutex_);
        const auto meter_seconds = std::chrono::duration<double>(now - meter_at_).count();
        if (meter_seconds >= 0.2) {
            current_eps_ = static_cast<double>(result.total_messages - meter_messages_) / meter_seconds;
            meter_messages_ = result.total_messages;
            meter_at_ = now;
        }
        result.current_eps = result.state == domain::GeneratorState::Running ? current_eps_ : 0.0;
    }
    if (result.elapsed_seconds > 0.0) {
        result.average_eps = static_cast<double>(result.total_messages) / result.elapsed_seconds;
    }
    {
        std::scoped_lock lock(error_mutex_);
        result.last_error = last_error_;
    }
    return result;
}

void StressTestService::run_worker(domain::GeneratorConfig config, std::vector<PreparedLog> logs, const std::uint32_t worker_index, const std::uint32_t worker_count, const std::stop_token stop_token) noexcept {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    std::uint64_t local_messages = 0;
    std::uint64_t local_bytes = 0;
    auto flush = [this, &local_messages, &local_bytes] {
        if (local_messages > 0) {
            total_messages_.fetch_add(local_messages, std::memory_order_relaxed);
            local_messages = 0;
        }
        if (local_bytes > 0) {
            total_bytes_.fetch_add(local_bytes, std::memory_order_relaxed);
            local_bytes = 0;
        }
    };

    try {
        auto transport = transport_factory_.create(config.endpoint.protocol);
        transport->connect(config.endpoint);
        const auto ready = connected_workers_.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (ready == worker_count && state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
            state_.store(domain::GeneratorState::Running, std::memory_order_release);
            logger_.info(std::format("All {} workers connected", worker_count));
        }

        const auto quota = quota_for_worker(config.target_eps, worker_index, worker_count);
        RatePacer pacer{quota};
        std::size_t log_index = worker_index % logs.size();
        if (transport->is_datagram()) {
            while (!stop_token.stop_requested()) {
                pacer.wait(1, stop_token);
                if (stop_token.stop_requested()) {
                    break;
                }
                const auto payload = logs[log_index].render(std::chrono::system_clock::now());
                transport->send(payload);
                ++local_messages;
                local_bytes += payload.size();
                log_index = (log_index + 1) % logs.size();
                if (local_messages >= 1024) {
                    flush();
                }
            }
        } else {
            const auto event_limit = stream_batch_events(quota);
            std::string batch;
            batch.reserve(262'144);
            while (!stop_token.stop_requested()) {
                batch.clear();
                std::size_t events = 0;
                while (events < event_limit && batch.size() < 262'144) {
                    const auto payload = logs[log_index].render(std::chrono::system_clock::now());
                    append_frame(batch, payload, config.endpoint.framing);
                    log_index = (log_index + 1) % logs.size();
                    ++events;
                }
                pacer.wait(events, stop_token);
                if (stop_token.stop_requested()) {
                    break;
                }
                transport->send(batch);
                local_messages += events;
                local_bytes += batch.size();
                if (local_messages >= 1024) {
                    flush();
                }
            }
        }
        flush();
    } catch (const std::exception& error) {
        flush();
        send_errors_.fetch_add(1, std::memory_order_relaxed);
        publish_error(error.what());
    } catch (...) {
        flush();
        send_errors_.fetch_add(1, std::memory_order_relaxed);
        publish_error("Unknown transmission failure");
    }
}

void StressTestService::publish_error(std::string message) noexcept {
    logger_.error(std::format("Stress test failure: {}", message));
    {
        std::scoped_lock lock(error_mutex_);
        if (last_error_.empty()) {
            last_error_ = std::move(message);
        }
    }
    state_.store(domain::GeneratorState::Failed, std::memory_order_release);
    stop_source_.request_stop();
}

}
