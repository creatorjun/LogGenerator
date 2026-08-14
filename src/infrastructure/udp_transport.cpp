// src/infrastructure/udp_transport.cpp
#include "infrastructure/udp_transport.hpp"

#include <stdexcept>

namespace loggen::infrastructure {

UdpTransport::UdpTransport(const SocketRuntime& runtime) noexcept
    : runtime_(runtime) {
}

void UdpTransport::connect(const domain::EndpointConfig& endpoint) {
    socket_ = connect_socket(runtime_, endpoint.host, endpoint.port, SOCK_DGRAM, IPPROTO_UDP);
    configure_send_buffer(socket_.get(), 4 * 1024 * 1024);
    configure_udp_stream(socket_.get());
}

application::SendResult UdpTransport::send(const std::string_view payload) {
    if (payload.size() > 65'507) {
        throw std::runtime_error("UDP payload exceeds 65,507 bytes");
    }
    send_datagram(socket_.get(), payload);
    return application::SendResult::Sent;
}

bool UdpTransport::is_datagram() const noexcept {
    return true;
}

}
