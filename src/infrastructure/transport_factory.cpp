// src/infrastructure/transport_factory.cpp
#include "infrastructure/transport_factory.hpp"

#include "infrastructure/file_transport.hpp"
#include "infrastructure/schannel_transport.hpp"
#include "infrastructure/tcp_transport.hpp"
#include "infrastructure/udp_transport.hpp"

#include <stdexcept>
#include <utility>

namespace loggen::infrastructure {

TransportFactory::TransportFactory(std::filesystem::path generated_directory)
    : generated_directory_(std::move(generated_directory)) {
}

std::unique_ptr<application::ILogTransport> TransportFactory::create(const domain::TransportProtocol protocol) const {
    switch (protocol) {
    case domain::TransportProtocol::Udp:
        return std::make_unique<UdpTransport>(winsock_);
    case domain::TransportProtocol::Tcp:
        return std::make_unique<TcpTransport>(winsock_);
    case domain::TransportProtocol::Tls:
        return std::make_unique<SchannelTransport>(winsock_);
    case domain::TransportProtocol::File:
        return std::make_unique<FileTransport>(generated_directory_);
    }
    throw std::invalid_argument("Unsupported transport protocol");
}

}
