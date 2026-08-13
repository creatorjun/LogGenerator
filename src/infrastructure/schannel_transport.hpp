// src/infrastructure/schannel_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"

#include <memory>

namespace loggen::infrastructure {

class SchannelTransport final : public application::ILogTransport {
public:
    SchannelTransport();
    ~SchannelTransport() override;

    SchannelTransport(const SchannelTransport&) = delete;
    SchannelTransport& operator=(const SchannelTransport&) = delete;

    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
