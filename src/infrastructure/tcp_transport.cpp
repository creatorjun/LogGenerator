// src/infrastructure/tcp_transport.cpp
#include "infrastructure/tcp_transport.hpp"

namespace loggen::infrastructure {

TcpTransport::TcpTransport(const SocketRuntime& runtime) noexcept
    : runtime_(runtime) {
}

void TcpTransport::connect(const domain::EndpointConfig& endpoint) {
    socket_ = connect_socket(runtime_, endpoint.host, endpoint.port, SOCK_STREAM, IPPROTO_TCP);
    configure_send_buffer(socket_.get(), 4 * 1024 * 1024);
    configure_tcp_stream(socket_.get());
}

application::SendResult TcpTransport::send(const std::string_view payload) {
    send_all(socket_.get(), payload);
    return application::SendResult::Sent;
}

bool TcpTransport::is_datagram() const noexcept {
    return false;
}

}
