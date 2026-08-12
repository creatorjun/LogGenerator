// tests/transport_error_tests.cpp
#include "test_support.hpp"

#include "infrastructure/schannel_transport.hpp"
#include "infrastructure/winsock_support.hpp"

#include <stdexcept>
#include <string>

namespace loggen::tests {

void run_transport_error_tests() {
    infrastructure::SchannelTransport tls;
    bool disconnected_tls_rejected = false;
    try {
        tls.send("event");
    } catch (const std::runtime_error&) {
        disconnected_tls_rejected = true;
    }
    expect(disconnected_tls_rejected, "TLS transport accepted data before a handshake");

    bool invalid_endpoint_rejected = false;
    try {
        static_cast<void>(infrastructure::connect_socket("", 0, SOCK_STREAM, IPPROTO_TCP));
    } catch (const std::invalid_argument&) {
        invalid_endpoint_rejected = true;
    }
    expect(invalid_endpoint_rejected, "Socket connection accepted an empty endpoint");

    bool invalid_utf8_rejected = false;
    try {
        static_cast<void>(infrastructure::utf8_to_wide(std::string{"\xC3\x28", 2}));
    } catch (const std::runtime_error&) {
        invalid_utf8_rejected = true;
    }
    expect(invalid_utf8_rejected, "UTF-8 conversion accepted an invalid sequence");
}

}
