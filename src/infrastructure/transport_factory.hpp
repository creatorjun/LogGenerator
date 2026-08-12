// src/infrastructure/transport_factory.hpp
#pragma once

#include "application/ports/log_transport.hpp"

#include <filesystem>

namespace loggen::infrastructure {

class TransportFactory final : public application::ITransportFactory {
public:
    explicit TransportFactory(std::filesystem::path generated_directory);
    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(domain::TransportProtocol protocol) const override;

private:
    std::filesystem::path generated_directory_;
};

}
