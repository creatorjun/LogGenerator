// src/application/ports/log_transport.hpp
#pragma once

#include "domain/generator_config.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

namespace loggen::application {

enum class SendResult {
    Sent,
    TotalBytesLimitReached,
    FileCountLimitReached,
    DurationLimitReached
};

class ILogTransport {
public:
    virtual ~ILogTransport() = default;
    virtual void connect(const domain::EndpointConfig& endpoint) = 0;
    [[nodiscard]] virtual SendResult send(std::string_view payload) = 0;
    [[nodiscard]] virtual SendResult send_batch(const std::span<const std::string_view> payloads) {
        for (const auto payload : payloads) {
            const auto result = send(payload);
            if (result != SendResult::Sent) {
                return result;
            }
        }
        return SendResult::Sent;
    }
    [[nodiscard]] virtual std::size_t preferred_batch_size() const noexcept {
        return 1;
    }
    [[nodiscard]] virtual bool is_datagram() const noexcept = 0;
};

class ITransportFactory {
public:
    virtual ~ITransportFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<ILogTransport> create(domain::TransportProtocol protocol) const = 0;
};

}
