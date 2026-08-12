// src/infrastructure/tcp_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/winsock_support.hpp"

namespace loggen::infrastructure {

class TcpTransport final : public application::ILogTransport {
public:
    void connect(const domain::EndpointConfig& endpoint) override;
    void send(std::string_view payload) override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    SocketHandle socket_;
};

}
