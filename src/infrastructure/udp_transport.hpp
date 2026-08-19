// src/infrastructure/udp_transport.hpp
#pragma once

#include "application/ports/log_transport.hpp"
#include "infrastructure/socket_support.hpp"

namespace loggen::infrastructure {

class UdpTransport final : public application::ILogTransport {
public:
    explicit UdpTransport(const SocketRuntime& runtime) noexcept;
    void connect(const domain::EndpointConfig& endpoint) override;
    [[nodiscard]] application::SendResult send(std::string_view payload) override;
    [[nodiscard]] application::SendResult send_batch(std::span<const std::string_view> payloads) override;
    [[nodiscard]] std::size_t preferred_batch_size() const noexcept override;
    [[nodiscard]] bool is_datagram() const noexcept override;

private:
    const SocketRuntime& runtime_;
    SocketHandle socket_;
};

}
