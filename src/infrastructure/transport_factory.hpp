// src/infrastructure/transport_factory.hpp
#pragma once

#include "application/ports/log_transport.hpp"

namespace loggen::infrastructure {

class TransportFactory final : public application::ITransportFactory {
public:
    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(domain::TransportProtocol protocol) const override;
};

}
