// src/infrastructure/openssl_transport.cpp
#include "infrastructure/openssl_transport.hpp"

#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>

#include <array>
#include <stdexcept>
#include <string>

namespace loggen::infrastructure {
namespace {

std::runtime_error tls_error(const std::string_view action) {
    const unsigned long code = ERR_get_error();
    if (code == 0) {
        return std::runtime_error(std::string(action) + " failed");
    }
    std::array<char, 256> detail{};
    ERR_error_string_n(code, detail.data(), detail.size());
    return std::runtime_error(std::string(action) + " failed: " + detail.data());
}

bool is_ip_address(const std::string& value) noexcept {
    std::array<unsigned char, 16> address{};
    return inet_pton(AF_INET, value.c_str(), address.data()) == 1 || inet_pton(AF_INET6, value.c_str(), address.data()) == 1;
}

}

struct OpenSslTransport::Impl {
    SocketHandle socket;
    SSL_CTX* context{nullptr};
    SSL* session{nullptr};

    ~Impl() {
        if (session != nullptr) {
            static_cast<void>(SSL_shutdown(session));
            SSL_free(session);
        }
        if (context != nullptr) {
            SSL_CTX_free(context);
        }
    }

    void initialize(const std::string& identity, const bool verify_certificate) {
        context = SSL_CTX_new(TLS_client_method());
        if (context == nullptr) {
            throw tls_error("SSL_CTX_new");
        }
        SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        if (verify_certificate) {
            SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
            if (SSL_CTX_set_default_verify_paths(context) != 1) {
                throw tls_error("Loading system CA certificates");
            }
        } else {
            SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
        }
        session = SSL_new(context);
        if (session == nullptr) {
            throw tls_error("SSL_new");
        }
        if (!is_ip_address(identity) && SSL_set_tlsext_host_name(session, identity.c_str()) != 1) {
            throw tls_error("Setting TLS server name");
        }
        if (verify_certificate) {
            X509_VERIFY_PARAM* parameters = SSL_get0_param(session);
            const int configured = is_ip_address(identity)
                ? X509_VERIFY_PARAM_set1_ip_asc(parameters, identity.c_str())
                : X509_VERIFY_PARAM_set1_host(parameters, identity.c_str(), identity.size());
            if (configured != 1) {
                throw tls_error("Setting TLS certificate identity");
            }
        }
        if (SSL_set_fd(session, socket.get()) != 1) {
            throw tls_error("SSL_set_fd");
        }
        if (SSL_connect(session) != 1) {
            throw tls_error("TLS handshake");
        }
        if (verify_certificate && SSL_get_verify_result(session) != X509_V_OK) {
            throw std::runtime_error("TLS certificate verification failed");
        }
    }

    void send_encrypted(const std::string_view payload) {
        std::size_t offset = 0;
        while (offset < payload.size()) {
            std::size_t written = 0;
            if (SSL_write_ex(session, payload.data() + offset, payload.size() - offset, &written) != 1) {
                throw tls_error("TLS send");
            }
            if (written == 0) {
                throw std::runtime_error("TLS send made no progress");
            }
            offset += written;
        }
    }
};

OpenSslTransport::OpenSslTransport(const SocketRuntime& runtime)
    : runtime_(runtime), impl_(std::make_unique<Impl>()) {
    static_cast<void>(OPENSSL_init_ssl(0, nullptr));
}

OpenSslTransport::~OpenSslTransport() = default;

void OpenSslTransport::connect(const domain::EndpointConfig& endpoint) {
    impl_ = std::make_unique<Impl>();
    impl_->socket = connect_socket(runtime_, endpoint.host, endpoint.port, SOCK_STREAM, IPPROTO_TCP);
    configure_send_buffer(impl_->socket.get(), 4 * 1024 * 1024);
    configure_tcp_stream(impl_->socket.get());
    const auto& identity = endpoint.tls_server_name.empty() ? endpoint.host : endpoint.tls_server_name;
    impl_->initialize(identity, endpoint.verify_certificate);
}

application::SendResult OpenSslTransport::send(const std::string_view payload) {
    if (impl_->session == nullptr) {
        throw std::runtime_error("TLS transport is not connected");
    }
    impl_->send_encrypted(payload);
    return application::SendResult::Sent;
}

bool OpenSslTransport::is_datagram() const noexcept {
    return false;
}

}
