// src/infrastructure/openssl_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/socket_support.hpp"

#include <memory>

namespace loggen::infrastructure {

class OpenSslTransport final : public application::ILogTransport {
public:
    explicit OpenSslTransport(const SocketRuntime& runtime);
    ~OpenSslTransport() override;

    OpenSslTransport(const OpenSslTransport&) = delete;
    OpenSslTransport& operator=(const OpenSslTransport&) = delete;

    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    struct Impl;
    const SocketRuntime& runtime_;
    std::unique_ptr<Impl> impl_;
};

}
