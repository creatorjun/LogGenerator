// src/infrastructure/transport_factory.cpp
#include "infrastructure/transport_factory.hpp"

#include "infrastructure/file_transport.hpp"
#ifdef _WIN32
#include "infrastructure/schannel_transport.hpp"
#else
#include "infrastructure/openssl_transport.hpp"
#endif
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
        return std::make_unique<UdpTransport>(socket_runtime_);
    case domain::TransportProtocol::Tcp:
        return std::make_unique<TcpTransport>(socket_runtime_);
    case domain::TransportProtocol::Tls:
#ifdef _WIN32
        return std::make_unique<SchannelTransport>(socket_runtime_);
#else
        return std::make_unique<OpenSslTransport>(socket_runtime_);
#endif
    case domain::TransportProtocol::File:
        return std::make_unique<FileTransport>(generated_directory_);
    }
    throw std::invalid_argument("Unsupported transport protocol");
}

}
