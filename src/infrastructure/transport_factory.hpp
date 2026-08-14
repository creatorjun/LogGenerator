// src/infrastructure/transport_factory.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/socket_support.hpp"

#include <filesystem>

namespace loggen::infrastructure {

class TransportFactory final : public application::ITransportFactory {
public:
    explicit TransportFactory(std::filesystem::path generated_directory);
    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(domain::TransportProtocol protocol) const override;

private:
    SocketRuntime socket_runtime_;
    std::filesystem::path generated_directory_;
};

}
