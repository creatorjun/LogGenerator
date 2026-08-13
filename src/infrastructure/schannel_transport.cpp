// src/infrastructure/schannel_transport.cpp
#include "infrastructure/schannel_transport.hpp"

#include "infrastructure/winsock_support.hpp"

#include <Windows.h>
#include <winternl.h>
#include <Schannel.h>
#include <Security.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace loggen::infrastructure {
namespace {

std::runtime_error security_error(const std::string_view action, const SECURITY_STATUS status) {
    std::ostringstream message;
    message << action << " failed (0x" << std::hex << std::uppercase << static_cast<unsigned long>(status) << ')';
    return std::runtime_error(message.str());
}

void release_output_token(SecBuffer& buffer) noexcept {
    if (buffer.pvBuffer != nullptr) {
        FreeContextBuffer(buffer.pvBuffer);
        buffer.pvBuffer = nullptr;
        buffer.cbBuffer = 0;
    }
}

}

struct SchannelTransport::Impl {
    SocketHandle socket;
    CredHandle credentials{};
    CtxtHandle context{};
    bool credentials_ready{false};
    bool context_ready{false};
    SecPkgContext_StreamSizes sizes{};
    std::vector<std::byte> encrypted;
    std::string wire;

    ~Impl() {
        if (context_ready) {
            DeleteSecurityContext(&context);
        }
        if (credentials_ready) {
            FreeCredentialsHandle(&credentials);
        }
    }

    void acquire_credentials(const bool verify_certificate) {
        SCH_CREDENTIALS configuration{};
        configuration.dwVersion = SCH_CREDENTIALS_VERSION;
        configuration.dwFlags = SCH_USE_STRONG_CRYPTO | SCH_CRED_NO_DEFAULT_CREDS;
        if (verify_certificate) {
            configuration.dwFlags |= SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
        } else {
            configuration.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
        }
        TimeStamp expiry{};
        const SECURITY_STATUS status = AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W), SECPKG_CRED_OUTBOUND, nullptr, &configuration, nullptr, nullptr, &credentials, &expiry);
        if (status != SEC_E_OK) {
            throw security_error("AcquireCredentialsHandle", status);
        }
        credentials_ready = true;
    }

    void send_token(SecBuffer& output) {
        if (output.pvBuffer != nullptr && output.cbBuffer > 0) {
            send_all(socket.get(), std::string_view(static_cast<const char*>(output.pvBuffer), output.cbBuffer));
        }
        release_output_token(output);
    }

    void receive_into(std::vector<char>& input) {
        std::array<char, 16'384> buffer{};
        const int received = recv(socket.get(), buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received == SOCKET_ERROR) {
            throw std::runtime_error(socket_error_message("TLS handshake receive"));
        }
        if (received == 0) {
            throw std::runtime_error("Remote endpoint closed during TLS handshake");
        }
        input.insert(input.end(), buffer.begin(), buffer.begin() + received);
    }

    void handshake(const std::wstring& server_name, const bool verify_certificate) {
        DWORD attributes = 0;
        TimeStamp expiry{};
        DWORD request_flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
        if (!verify_certificate) {
            request_flags |= ISC_REQ_MANUAL_CRED_VALIDATION;
        }

        SecBuffer first_output{0, SECBUFFER_TOKEN, nullptr};
        SecBufferDesc first_output_description{SECBUFFER_VERSION, 1, &first_output};
        SECURITY_STATUS status = InitializeSecurityContextW(&credentials, nullptr, const_cast<wchar_t*>(server_name.c_str()), request_flags, 0, SECURITY_NATIVE_DREP, nullptr, 0, &context, &first_output_description, &attributes, &expiry);
        context_ready = status == SEC_I_CONTINUE_NEEDED || status == SEC_E_OK;
        try {
            send_token(first_output);
        } catch (...) {
            release_output_token(first_output);
            throw;
        }
        if (!context_ready) {
            throw security_error("InitializeSecurityContext", status);
        }

        std::vector<char> input;
        input.reserve(65'536);
        while (status != SEC_E_OK) {
            if (input.empty() || status == SEC_E_INCOMPLETE_MESSAGE) {
                receive_into(input);
            }
            SecBuffer input_buffers[2]{{static_cast<unsigned long>(input.size()), SECBUFFER_TOKEN, input.data()}, {0, SECBUFFER_EMPTY, nullptr}};
            SecBufferDesc input_description{SECBUFFER_VERSION, 2, input_buffers};
            SecBuffer output{0, SECBUFFER_TOKEN, nullptr};
            SecBufferDesc output_description{SECBUFFER_VERSION, 1, &output};
            status = InitializeSecurityContextW(&credentials, &context, const_cast<wchar_t*>(server_name.c_str()), request_flags, 0, SECURITY_NATIVE_DREP, &input_description, 0, nullptr, &output_description, &attributes, &expiry);
            if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
                const SECURITY_STATUS complete_status = CompleteAuthToken(&context, &output_description);
                if (complete_status != SEC_E_OK) {
                    release_output_token(output);
                    throw security_error("CompleteAuthToken", complete_status);
                }
                status = status == SEC_I_COMPLETE_NEEDED ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
            }
            try {
                send_token(output);
            } catch (...) {
                release_output_token(output);
                throw;
            }
            if (status == SEC_E_INCOMPLETE_MESSAGE) {
                continue;
            }
            if (status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED) {
                throw security_error("TLS handshake", status);
            }
            if (input_buffers[1].BufferType == SECBUFFER_EXTRA && input_buffers[1].cbBuffer > 0) {
                const auto extra = static_cast<std::size_t>(input_buffers[1].cbBuffer);
                std::vector<char> remaining(input.end() - static_cast<std::ptrdiff_t>(extra), input.end());
                input.swap(remaining);
            } else {
                input.clear();
            }
        }
        const SECURITY_STATUS sizes_status = QueryContextAttributesW(&context, SECPKG_ATTR_STREAM_SIZES, &sizes);
        if (sizes_status != SEC_E_OK) {
            throw security_error("QueryContextAttributes", sizes_status);
        }
        encrypted.reserve(static_cast<std::size_t>(sizes.cbHeader) + sizes.cbMaximumMessage + sizes.cbTrailer);
        wire.reserve(static_cast<std::size_t>(sizes.cbHeader) + sizes.cbMaximumMessage + sizes.cbTrailer);
    }

    void send_encrypted(const std::string_view payload) {
        std::size_t offset = 0;
        while (offset < payload.size()) {
            const auto chunk_size = std::min<std::size_t>(payload.size() - offset, sizes.cbMaximumMessage);
            const auto storage_size = static_cast<std::size_t>(sizes.cbHeader) + chunk_size + sizes.cbTrailer;
            encrypted.resize(storage_size);
            std::memcpy(encrypted.data() + sizes.cbHeader, payload.data() + offset, chunk_size);

            SecBuffer buffers[4]{
                {sizes.cbHeader, SECBUFFER_STREAM_HEADER, encrypted.data()},
                {static_cast<unsigned long>(chunk_size), SECBUFFER_DATA, encrypted.data() + sizes.cbHeader},
                {sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, encrypted.data() + sizes.cbHeader + chunk_size},
                {0, SECBUFFER_EMPTY, nullptr},
            };
            SecBufferDesc description{SECBUFFER_VERSION, 4, buffers};
            const SECURITY_STATUS status = EncryptMessage(&context, 0, &description, 0);
            if (status != SEC_E_OK) {
                throw security_error("EncryptMessage", status);
            }
            wire.clear();
            wire.append(static_cast<const char*>(buffers[0].pvBuffer), buffers[0].cbBuffer);
            wire.append(static_cast<const char*>(buffers[1].pvBuffer), buffers[1].cbBuffer);
            wire.append(static_cast<const char*>(buffers[2].pvBuffer), buffers[2].cbBuffer);
            send_all(socket.get(), wire);
            offset += chunk_size;
        }
    }
};

SchannelTransport::SchannelTransport()
    : impl_(std::make_unique<Impl>()) {
}

SchannelTransport::~SchannelTransport() = default;

void SchannelTransport::connect(const domain::EndpointConfig& endpoint) {
    impl_->socket = connect_socket(endpoint.host, endpoint.port, SOCK_STREAM, IPPROTO_TCP);
    configure_send_buffer(impl_->socket.get(), 4 * 1024 * 1024);
    BOOL enabled = TRUE;
    if (setsockopt(impl_->socket.get(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == SOCKET_ERROR) {
        throw std::runtime_error(socket_error_message("setsockopt TCP_NODELAY"));
    }
    impl_->acquire_credentials(endpoint.verify_certificate);
    const auto identity = endpoint.tls_server_name.empty() ? endpoint.host : endpoint.tls_server_name;
    impl_->handshake(utf8_to_wide(identity), endpoint.verify_certificate);
}

application::SendResult SchannelTransport::send(const std::string_view payload) {
    impl_->send_encrypted(payload);
    return application::SendResult::Sent;
}

bool SchannelTransport::is_datagram() const noexcept {
    return false;
}

}
