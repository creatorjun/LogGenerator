// src/infrastructure/tcp_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/winsock_support.hpp"

namespace loggen::infrastructure {

class TcpTransport final : public application::ILogTransport {
public:
    explicit TcpTransport(const WinsockRuntime& runtime) noexcept;
    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    const WinsockRuntime& runtime_;
    SocketHandle socket_;
};

}
