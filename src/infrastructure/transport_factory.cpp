// src/infrastructure/transport_factory.cpp
#include "infrastructure/transport_factory.hpp"

#include "infrastructure/schannel_transport.hpp"
#include "infrastructure/tcp_transport.hpp"
#include "infrastructure/udp_transport.hpp"

#include <stdexcept>

namespace loggen::infrastructure {

std::unique_ptr<application::ILogTransport> TransportFactory::create(const domain::TransportProtocol protocol) const {
    switch (protocol) {
    case domain::TransportProtocol::Udp:
        return std::make_unique<UdpTransport>();
    case domain::TransportProtocol::Tcp:
        return std::make_unique<TcpTransport>();
    case domain::TransportProtocol::Tls:
        return std::make_unique<SchannelTransport>();
    }
    throw std::invalid_argument("Unsupported transport protocol");
}

}
