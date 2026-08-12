// src/application/ports/log_transport.hpp
#pragma once

#include "domain/generator_config.hpp"

#include <memory>
#include <string_view>

namespace loggen::application {

class ILogTransport {
public:
    virtual ~ILogTransport() = default;
    virtual void connect(const domain::EndpointConfig& endpoint) = 0;
    virtual void send(std::string_view payload) = 0;
    [[nodiscard]] virtual bool is_datagram() const noexcept = 0;
};

class ITransportFactory {
public:
    virtual ~ITransportFactory() = default;
    [[nodiscard]] virtual std::unique_ptr<ILogTransport> create(domain::TransportProtocol protocol) const = 0;
};

}
