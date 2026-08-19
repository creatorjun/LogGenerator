// src/infrastructure/transport_factory.cpp
#include "infrastructure/transport_factory.hpp"

#include "infrastructure/file_transport.hpp"
#include "infrastructure/socket_support.hpp"
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

class TransportFactory::Impl final {
public:
    explicit Impl(std::filesystem::path directory)
        : generated_directory(std::move(directory)) {
    }

    SocketRuntime socket_runtime;
    std::filesystem::path generated_directory;
};

TransportFactory::TransportFactory(std::filesystem::path generated_directory)
    : impl_(std::make_unique<Impl>(std::move(generated_directory))) {
}

TransportFactory::~TransportFactory() = default;

std::unique_ptr<application::ILogTransport> TransportFactory::create(const domain::TransportProtocol protocol) const {
    switch (protocol) {
    case domain::TransportProtocol::Udp:
        return std::make_unique<UdpTransport>(impl_->socket_runtime);
    case domain::TransportProtocol::Tcp:
        return std::make_unique<TcpTransport>(impl_->socket_runtime);
    case domain::TransportProtocol::Tls:
#ifdef _WIN32
        return std::make_unique<SchannelTransport>(impl_->socket_runtime);
#else
        return std::make_unique<OpenSslTransport>(impl_->socket_runtime);
#endif
    case domain::TransportProtocol::File:
        return std::make_unique<FileTransport>(impl_->generated_directory);
    }
    throw std::invalid_argument("Unsupported transport protocol");
}

}
