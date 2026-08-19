// src/infrastructure/udp_transport.cpp
#include "infrastructure/udp_transport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <limits>
#include <stdexcept>

#if defined(__APPLE__)
#include <sys/uio.h>

namespace {

// XNU exports sendmsg_x from libSystem but keeps the declaration and
// msghdr_x layout behind its private SDK guard. Keep the ABI declaration
// local to the macOS adapter and fall back to send() if it is unavailable.
struct DarwinMessageHeaderX {
    void* msg_name;
    socklen_t msg_namelen;
    iovec* msg_iov;
    int msg_iovlen;
    void* msg_control;
    socklen_t msg_controllen;
    int msg_flags;
    std::size_t msg_datalen;
};

extern "C" ssize_t sendmsg_x(int socket, void* messages, unsigned int count, int flags) __attribute__((weak_import));

constexpr std::size_t maximum_datagram_batch{256};

}
#elif defined(__linux__)
#include <sys/socket.h>
#include <sys/uio.h>

namespace {
constexpr std::size_t maximum_datagram_batch{256};
}
#endif

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

application::SendResult UdpTransport::send_batch(const std::span<const std::string_view> payloads) {
    if (payloads.empty()) {
        return application::SendResult::Sent;
    }
    for (const auto payload : payloads) {
        if (payload.size() > 65'507) {
            throw std::runtime_error("UDP payload exceeds 65,507 bytes");
        }
    }

#if defined(__APPLE__)
    if (sendmsg_x != nullptr) {
        std::array<iovec, maximum_datagram_batch> vectors{};
        std::array<DarwinMessageHeaderX, maximum_datagram_batch> messages{};
        std::size_t offset = 0;
        while (offset < payloads.size()) {
            const auto count = std::min(maximum_datagram_batch, payloads.size() - offset);
            for (std::size_t index = 0; index < count; ++index) {
                const auto payload = payloads[offset + index];
                vectors[index].iov_base = const_cast<char*>(payload.data());
                vectors[index].iov_len = payload.size();
                messages[index].msg_iov = &vectors[index];
                messages[index].msg_iovlen = 1;
            }
            const auto sent = sendmsg_x(socket_.get(), messages.data(), static_cast<unsigned int>(count), 0);
            if (sent < 0) {
                if (errno == ECONNREFUSED) {
                    offset += count;
                    continue;
                }
                throw std::runtime_error(socket_error_message("UDP sendmsg_x"));
            }
            if (sent == 0 || static_cast<std::size_t>(sent) > count) {
                throw std::runtime_error("UDP sendmsg_x returned an invalid datagram count");
            }
            offset += static_cast<std::size_t>(sent);
        }
        return application::SendResult::Sent;
    }
#elif defined(__linux__)
    std::array<iovec, maximum_datagram_batch> vectors{};
    std::array<mmsghdr, maximum_datagram_batch> messages{};
    std::size_t offset = 0;
    while (offset < payloads.size()) {
        const auto count = std::min(maximum_datagram_batch, payloads.size() - offset);
        for (std::size_t index = 0; index < count; ++index) {
            const auto payload = payloads[offset + index];
            vectors[index].iov_base = const_cast<char*>(payload.data());
            vectors[index].iov_len = payload.size();
            messages[index].msg_hdr.msg_iov = &vectors[index];
            messages[index].msg_hdr.msg_iovlen = 1;
            messages[index].msg_len = 0;
        }
        const auto sent = ::sendmmsg(socket_.get(), messages.data(), static_cast<unsigned int>(count), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == ECONNREFUSED) {
                offset += count;
                continue;
            }
            throw std::runtime_error(socket_error_message("UDP sendmmsg"));
        }
        if (sent == 0 || static_cast<std::size_t>(sent) > count) {
            throw std::runtime_error("UDP sendmmsg returned an invalid datagram count");
        }
        offset += static_cast<std::size_t>(sent);
    }
    return application::SendResult::Sent;
#endif

    return application::ILogTransport::send_batch(payloads);
}

std::size_t UdpTransport::preferred_batch_size() const noexcept {
#if defined(__APPLE__) || defined(__linux__)
    return maximum_datagram_batch;
#else
    return 1;
#endif
}

bool UdpTransport::is_datagram() const noexcept {
    return true;
}

}
