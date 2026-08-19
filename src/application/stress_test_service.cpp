// src/application/stress_test_service.cpp
#include "application/stress_test_service.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <format>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace loggen::application {
namespace {

class RatePacer {
public:
    RatePacer(const IExecutionRuntime& execution_runtime, const std::uint64_t events_per_second)
        : execution_runtime_(execution_runtime), events_per_second_(events_per_second), next_(std::chrono::steady_clock::now()) {
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
            execution_runtime_.pause_current_thread();
        }
    }

private:
    const IExecutionRuntime& execution_runtime_;
    std::uint64_t events_per_second_{0};
    std::chrono::steady_clock::time_point next_;
};

class CachedSystemClock {
public:
    explicit CachedSystemClock(const bool high_rate)
        : high_rate_(high_rate), refresh_countdown_(high_rate ? 255U : 0U), cached_(std::chrono::system_clock::now()) {
    }

    [[nodiscard]] std::chrono::system_clock::time_point now() {
        if (!high_rate_ || refresh_countdown_ == 0) {
            cached_ = std::chrono::system_clock::now();
            refresh_countdown_ = high_rate_ ? 255U : 0U;
        } else {
            --refresh_countdown_;
        }
        return cached_;
    }

private:
    bool high_rate_{false};
    std::uint32_t refresh_countdown_{0};
    std::chrono::system_clock::time_point cached_;
};

class TimestampCursor {
public:
    TimestampCursor(const domain::TimestampGeneration& generation, const std::uint32_t worker_index, const std::uint32_t worker_count, const bool high_rate)
        : generation_(generation), position_(worker_index), step_(worker_count), system_clock_(high_rate) {
        if (generation_.mode == domain::TimestampGenerationMode::Range) {
            span_ = generation_.range.inclusive_seconds();
            if (span_ == 0) {
                throw std::invalid_argument("Timestamp range is invalid");
            }
            position_ %= span_;
            step_ %= span_;
        }
    }

    [[nodiscard]] std::chrono::system_clock::time_point next() {
        if (generation_.mode == domain::TimestampGenerationMode::Offset) {
            return system_clock_.now();
        }
        const auto result = generation_.range.start + std::chrono::seconds{position_};
        position_ += step_;
        if (position_ >= span_) {
            position_ -= span_;
        }
        return result;
    }

    [[nodiscard]] bool calendar_time() const noexcept {
        return generation_.mode == domain::TimestampGenerationMode::Range;
    }

private:
    domain::TimestampGeneration generation_;
    std::uint64_t position_{0};
    std::uint64_t step_{1};
    std::uint64_t span_{0};
    CachedSystemClock system_clock_;
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
    if (payload.find_first_of("\r\n") == std::string_view::npos) {
        batch.append(payload);
        batch.push_back('\n');
        return;
    }
    for (const auto value : payload) {
        if (value == '\r') {
            batch.append("\\r");
        } else if (value == '\n') {
            batch.append("\\n");
        } else {
            batch.push_back(value);
        }
    }
    batch.push_back('\n');
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
        return 1024;
    }
    return static_cast<std::size_t>(std::clamp<std::uint64_t>(quota / 200, 1, 1024));
}

std::size_t file_batch_events(const std::uint64_t quota, const domain::EndpointConfig& endpoint) {
    if (endpoint.file_max_total_bytes > 0 || endpoint.file_max_count > 0 || endpoint.file_max_duration.count() > 0) {
        return 1;
    }
    if (quota == 0) {
        return 256;
    }
    return static_cast<std::size_t>(std::clamp<std::uint64_t>(quota / 20, 1, 256));
}

std::string_view completion_message(const SendResult result) {
    switch (result) {
    case SendResult::TotalBytesLimitReached:
        return "FILE 총 생성량 제한에 도달하여 자동 중지했습니다.";
    case SendResult::FileCountLimitReached:
        return "FILE 생성 파일 개수 제한에 도달하여 자동 중지했습니다.";
    case SendResult::DurationLimitReached:
        return "FILE 실행 시간 제한에 도달하여 자동 중지했습니다.";
    case SendResult::Sent:
        return {};
    }
    return "FILE 생성 제한에 도달하여 자동 중지했습니다.";
}

}

StressTestService::StressTestService(const ITransportFactory& transport_factory, const IExecutionRuntime& execution_runtime, ILogger& logger)
    : transport_factory_(transport_factory), execution_runtime_(execution_runtime), logger_(logger) {
}

StressTestService::~StressTestService() {
    stop();
}

void StressTestService::start(domain::GeneratorConfig config) {
    stop();
    if (config.templates.empty()) {
        throw std::invalid_argument("At least one log template is required");
    }
    if (config.endpoint.protocol != domain::TransportProtocol::File && (config.endpoint.host.empty() || config.endpoint.port == 0)) {
        throw std::invalid_argument("A destination host and port are required");
    }
    if (config.timestamp_generation.mode == domain::TimestampGenerationMode::Range && !config.timestamp_generation.range.valid()) {
        throw std::invalid_argument("A valid timestamp range is required");
    }
    config.worker_count = std::clamp<std::uint32_t>(config.worker_count, 1, 64);
    if (config.endpoint.protocol == domain::TransportProtocol::File) {
        config.worker_count = 1;
        config.endpoint.framing = domain::StreamFraming::Newline;
    }
    if (config.target_eps > 0) {
        config.worker_count = static_cast<std::uint32_t>(std::min<std::uint64_t>(config.worker_count, config.target_eps));
    }
    const auto endpoint = config.endpoint.protocol == domain::TransportProtocol::File ? std::string{"generated directory"} : std::format("{}:{}", config.endpoint.host, config.endpoint.port);
    logger_.info(std::format(
        "Stress test starting: protocol={}, endpoint={}, workers={}, templates={}, target_eps={}",
        domain::protocol_name(config.endpoint.protocol),
        endpoint,
        config.worker_count,
        config.templates.size(),
        config.target_eps == 0 ? std::string("unlimited") : std::to_string(config.target_eps)));

    {
        std::scoped_lock lock(lifecycle_mutex_);
        total_messages_.store(0, std::memory_order_relaxed);
        total_bytes_.store(0, std::memory_order_relaxed);
        send_errors_.store(0, std::memory_order_relaxed);
        connected_workers_.store(0, std::memory_order_relaxed);
        {
            std::scoped_lock error_lock(error_mutex_);
            last_error_.clear();
            status_message_.clear();
        }
        stop_source_ = std::stop_source{};
        started_at_ = std::chrono::steady_clock::now();
        {
            std::scoped_lock meter_lock(meter_mutex_);
            meter_at_ = started_at_;
            meter_progress_at_ = started_at_;
            meter_messages_ = 0;
            meter_idle_seconds_ = 3.0;
            current_eps_ = 0.0;
        }
        state_.store(domain::GeneratorState::Connecting, std::memory_order_release);
        const auto stop_token = stop_source_.get_token();
        supervisor_ = std::jthread([this, config = std::move(config), stop_token](std::stop_token) mutable {
            run_supervisor(std::move(config), stop_token);
        });
    }
}

void StressTestService::request_stop() noexcept {
    std::scoped_lock lock(lifecycle_mutex_);
    if (!supervisor_.joinable()) {
        return;
    }
    if (state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
        state_.store(domain::GeneratorState::Stopping, std::memory_order_release);
    }
    stop_source_.request_stop();
}

void StressTestService::stop() noexcept {
    std::jthread supervisor;
    {
        std::scoped_lock lock(lifecycle_mutex_);
        if (!supervisor_.joinable()) {
            state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
            std::scoped_lock meter_lock(meter_mutex_);
            current_eps_ = 0.0;
            return;
        }
        if (state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
            state_.store(domain::GeneratorState::Stopping, std::memory_order_release);
        }
        stop_source_.request_stop();
        supervisor = std::move(supervisor_);
    }
    if (supervisor.joinable()) {
        supervisor.join();
    }
    state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
    {
        std::scoped_lock meter_lock(meter_mutex_);
        current_eps_ = 0.0;
    }
    try {
        logger_.info(std::format(
            "Stress test stopped: messages={}, bytes={}, errors={}",
            total_messages_.load(std::memory_order_relaxed),
            total_bytes_.load(std::memory_order_relaxed),
            send_errors_.load(std::memory_order_relaxed)));
    } catch (...) {
        logger_.info("Stress test stopped");
    }
}

domain::TransmissionStats StressTestService::snapshot() {
    domain::TransmissionStats result;
    result.state = state_.load(std::memory_order_acquire);
    result.total_messages = total_messages_.load(std::memory_order_relaxed);
    result.total_bytes = total_bytes_.load(std::memory_order_relaxed);
    result.send_errors = send_errors_.load(std::memory_order_relaxed);
    const auto now = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point started_at;
    {
        std::scoped_lock lock(lifecycle_mutex_);
        started_at = started_at_;
    }
    if (started_at.time_since_epoch().count() != 0) {
        result.elapsed_seconds = std::chrono::duration<double>(now - started_at).count();
    }
    {
        std::scoped_lock lock(meter_mutex_);
        const auto meter_seconds = std::chrono::duration<double>(now - meter_at_).count();
        if (result.total_messages < meter_messages_) {
            meter_messages_ = result.total_messages;
            meter_at_ = now;
            meter_progress_at_ = now;
            meter_idle_seconds_ = 3.0;
            current_eps_ = 0.0;
        } else if (result.total_messages > meter_messages_ && meter_seconds >= 0.2) {
            current_eps_ = static_cast<double>(result.total_messages - meter_messages_) / meter_seconds;
            meter_messages_ = result.total_messages;
            meter_at_ = now;
            meter_progress_at_ = now;
            meter_idle_seconds_ = std::max(3.0, meter_seconds * 3.0);
        } else if (result.total_messages == meter_messages_ && current_eps_ > 0.0 && std::chrono::duration<double>(now - meter_progress_at_).count() >= meter_idle_seconds_) {
            current_eps_ = 0.0;
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
        result.status_message = status_message_;
    }
    return result;
}

void StressTestService::run_supervisor(domain::GeneratorConfig config, const std::stop_token stop_token) noexcept {
    try {
        const auto timer_resolution = execution_runtime_.acquire_high_resolution_timer();
        static_cast<void>(timer_resolution);
        auto prepared = LogRenderer::prepare(config);
        if (stop_token.stop_requested()) {
            state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
            return;
        }

        std::vector<std::jthread> workers;
        workers.reserve(config.worker_count);
        auto round_robin = std::make_shared<RoundRobinCursor>(prepared.size());
        for (std::uint32_t worker_index = 0; worker_index < config.worker_count; ++worker_index) {
            auto worker_logs = prepared;
            const auto quota = quota_for_worker(config.target_eps, worker_index, config.worker_count);
            workers.emplace_back([this, endpoint = config.endpoint, timestamp_generation = config.timestamp_generation, logs = std::move(worker_logs), round_robin, quota, worker_index, worker_count = config.worker_count](const std::stop_token worker_stop_token) mutable {
                run_worker(std::move(endpoint), timestamp_generation, std::move(logs), round_robin, quota, worker_index, worker_count, worker_stop_token);
            });
        }
        std::mutex stop_wait_mutex;
        std::condition_variable stop_wait_condition;
        std::stop_callback stop_callback{stop_token, [&stop_wait_mutex, &stop_wait_condition] {
            {
                const std::scoped_lock lock(stop_wait_mutex);
            }
            stop_wait_condition.notify_all();
        }};
        std::unique_lock stop_wait_lock(stop_wait_mutex);
        stop_wait_condition.wait(stop_wait_lock, [stop_token] {
            return stop_token.stop_requested();
        });
        stop_wait_lock.unlock();
        for (auto& worker : workers) {
            worker.request_stop();
        }
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        if (state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
            state_.store(domain::GeneratorState::Stopped, std::memory_order_release);
        }
    } catch (const std::exception& error) {
        publish_error(error.what());
    } catch (...) {
        publish_error("Unknown worker initialization failure");
    }
}

void StressTestService::run_worker(domain::EndpointConfig endpoint, const domain::TimestampGeneration timestamp_generation, std::vector<PreparedLog> logs, std::shared_ptr<RoundRobinCursor> round_robin, const std::uint64_t quota, const std::uint32_t worker_index, const std::uint32_t worker_count, const std::stop_token stop_token) noexcept {
    execution_runtime_.configure_current_worker();
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
        auto transport = transport_factory_.create(endpoint.protocol);
        transport->connect(endpoint);
        const auto ready = connected_workers_.fetch_add(1, std::memory_order_acq_rel) + 1;
        auto expected_state = domain::GeneratorState::Connecting;
        if (ready == worker_count && state_.compare_exchange_strong(expected_state, domain::GeneratorState::Running, std::memory_order_acq_rel)) {
            try {
                logger_.info(std::format("All {} workers connected", worker_count));
            } catch (...) {
                logger_.info("All workers connected");
            }
        }

        RatePacer pacer{execution_runtime_, quota};
        TimestampCursor clock{timestamp_generation, worker_index, worker_count, quota == 0 || quota >= 10'000};
        const bool calendar_time = clock.calendar_time();
        if (transport->is_datagram()) {
            while (!stop_token.stop_requested()) {
                pacer.wait(1, stop_token);
                if (stop_token.stop_requested()) {
                    break;
                }
                const auto payload = logs[round_robin->next()].render(clock.next(), calendar_time);
                const auto send_result = transport->send(payload);
                if (send_result != SendResult::Sent) {
                    flush();
                    publish_completion(std::string{completion_message(send_result)});
                    return;
                }
                ++local_messages;
                local_bytes += payload.size();
                if (local_messages >= 1024) {
                    flush();
                }
            }
        } else {
            const auto event_limit = endpoint.protocol == domain::TransportProtocol::File ? file_batch_events(quota, endpoint) : stream_batch_events(quota);
            std::string batch;
            const std::size_t batch_limit = endpoint.protocol == domain::TransportProtocol::File ? 65'536 : 1'048'576;
            batch.reserve(batch_limit);
            while (!stop_token.stop_requested()) {
                batch.clear();
                std::size_t events = 0;
                const auto batch_timestamp = calendar_time ? std::chrono::system_clock::time_point{} : clock.next();
                while (events < event_limit && batch.size() < batch_limit) {
                    const auto timestamp = calendar_time ? clock.next() : batch_timestamp;
                    const auto payload = logs[round_robin->next()].render(timestamp, calendar_time);
                    append_frame(batch, payload, endpoint.framing);
                    ++events;
                }
                pacer.wait(events, stop_token);
                if (stop_token.stop_requested()) {
                    break;
                }
                const auto send_result = transport->send(batch);
                if (send_result != SendResult::Sent) {
                    flush();
                    publish_completion(std::string{completion_message(send_result)});
                    return;
                }
                local_messages += events;
                local_bytes += batch.size();
                if (endpoint.protocol == domain::TransportProtocol::File || local_messages >= 1024) {
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
    try {
        logger_.error(std::format("Stress test failure: {}", message));
    } catch (...) {
        logger_.error(message);
    }
    {
        std::scoped_lock lock(error_mutex_);
        if (last_error_.empty()) {
            last_error_ = std::move(message);
        }
    }
    state_.store(domain::GeneratorState::Failed, std::memory_order_release);
    stop_source_.request_stop();
}

void StressTestService::publish_completion(std::string message) noexcept {
    try {
        logger_.info(message);
    } catch (...) {
    }
    {
        std::scoped_lock lock(error_mutex_);
        status_message_ = std::move(message);
    }
    if (state_.load(std::memory_order_acquire) != domain::GeneratorState::Failed) {
        state_.store(domain::GeneratorState::Stopping, std::memory_order_release);
    }
    stop_source_.request_stop();
}

}
