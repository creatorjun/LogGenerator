// src/infrastructure/schannel_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/winsock_support.hpp"

#include <memory>

namespace loggen::infrastructure {

class SchannelTransport final : public application::ILogTransport {
public:
    explicit SchannelTransport(const WinsockRuntime& runtime);
    ~SchannelTransport() override;

    SchannelTransport(const SchannelTransport&) = delete;
    SchannelTransport& operator=(const SchannelTransport&) = delete;

    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    struct Impl;
    const WinsockRuntime& runtime_;
    std::unique_ptr<Impl> impl_;
};

}
