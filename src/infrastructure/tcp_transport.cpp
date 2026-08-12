// src/infrastructure/tcp_transport.cpp
#include "infrastructure/tcp_transport.hpp"

#include <WS2tcpip.h>

#include <stdexcept>

namespace loggen::infrastructure {

void TcpTransport::connect(const domain::EndpointConfig& endpoint) {
    socket_ = connect_socket(endpoint.host, endpoint.port, SOCK_STREAM, IPPROTO_TCP);
    configure_send_buffer(socket_.get(), 4 * 1024 * 1024);
    BOOL enabled = TRUE;
    if (setsockopt(socket_.get(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("setsockopt TCP_NODELAY"));
    }
    setsockopt(socket_.get(), SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
}

void TcpTransport::send(const std::string_view payload) {
    send_all(socket_.get(), payload);
}

bool TcpTransport::is_datagram() const noexcept {
    return false;
}

}
