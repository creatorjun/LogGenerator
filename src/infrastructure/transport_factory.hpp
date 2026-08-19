// src/infrastructure/transport_factory.hpp
#pragma once

#include "application/ports/log_transport.hpp"

#include <filesystem>
#include <memory>

namespace loggen::infrastructure {

class TransportFactory final : public application::ITransportFactory {
public:
    explicit TransportFactory(std::filesystem::path generated_directory);
    ~TransportFactory() override;

    TransportFactory(const TransportFactory&) = delete;
    TransportFactory& operator=(const TransportFactory&) = delete;

    [[nodiscard]] std::unique_ptr<application::ILogTransport> create(domain::TransportProtocol protocol) const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
