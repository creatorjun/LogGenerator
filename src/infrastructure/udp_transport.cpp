// src/infrastructure/udp_transport.cpp
#include "infrastructure/udp_transport.hpp"

#include <MSWSock.h>

#include <limits>
#include <stdexcept>

namespace loggen::infrastructure {

void UdpTransport::connect(const domain::EndpointConfig& endpoint) {
    socket_ = connect_socket(endpoint.host, endpoint.port, SOCK_DGRAM, IPPROTO_UDP);
    configure_send_buffer(socket_.get(), 4 * 1024 * 1024);
    BOOL disabled = FALSE;
    DWORD returned = 0;
    if (WSAIoctl(socket_.get(), SIO_UDP_CONNRESET, &disabled, sizeof(disabled), nullptr, 0, &returned, nullptr, nullptr) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("WSAIoctl SIO_UDP_CONNRESET"));
    }
}

void UdpTransport::send(const std::string_view payload) {
    if (payload.size() > 65'507) {
        throw std::runtime_error("UDP payload exceeds 65,507 bytes");
    }
    const int sent = ::send(socket_.get(), payload.data(), static_cast<int>(payload.size()), 0);
    if (sent == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("UDP send"));
    }
    if (sent != static_cast<int>(payload.size())) {
        throw std::runtime_error("UDP send was incomplete");
    }
}

bool UdpTransport::is_datagram() const noexcept {
    return true;
}

}
