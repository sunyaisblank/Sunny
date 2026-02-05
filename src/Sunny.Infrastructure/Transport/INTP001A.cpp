/**
 * @file INTP001A.cpp
 * @brief Network Transport implementation
 *
 * Component: INTP001A
 *
 * Note: This is a stub implementation. Production code would
 * use asio, libuv, or platform-specific socket APIs.
 */

#include "INTP001A.h"

namespace Sunny::Infrastructure {

// =============================================================================
// TcpTransport
// =============================================================================

TcpTransport::TcpTransport(const TransportConfig& config)
    : config_(config) {}

TcpTransport::~TcpTransport() {
    disconnect();
}

bool TcpTransport::connect() {
    if (state_ == TransportState::Connected) {
        return true;
    }

    set_state(TransportState::Connecting, "Connecting to " +
              config_.host + ":" + std::to_string(config_.tcp_port));

    // Stub: In production, this would create a TCP socket and connect
    // For now, simulate a successful connection for local testing
    set_state(TransportState::Connected, "Connected");
    return true;
}

void TcpTransport::disconnect() {
    if (state_ == TransportState::Disconnected) {
        return;
    }

    // Stub: Close socket
    set_state(TransportState::Disconnected, "Disconnected");
}

bool TcpTransport::send(const std::string& /* message */) {
    if (state_ != TransportState::Connected) {
        return false;
    }

    // Stub: Write to socket
    return true;
}

void TcpTransport::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void TcpTransport::set_state_callback(StateCallback callback) {
    state_callback_ = std::move(callback);
}

TransportState TcpTransport::state() const {
    return state_;
}

bool TcpTransport::is_connected() const {
    return state_ == TransportState::Connected;
}

void TcpTransport::set_state(TransportState new_state, const std::string& message) {
    if (state_ == new_state) {
        return;
    }

    state_ = new_state;
    if (state_callback_) {
        state_callback_(state_, message);
    }
}

// =============================================================================
// UdpTransport
// =============================================================================

UdpTransport::UdpTransport(const TransportConfig& config)
    : config_(config) {}

UdpTransport::~UdpTransport() {
    disconnect();
}

bool UdpTransport::connect() {
    // UDP is connectionless, but we can "connect" to set default destination
    state_ = TransportState::Connected;
    return true;
}

void UdpTransport::disconnect() {
    state_ = TransportState::Disconnected;
}

bool UdpTransport::send(const std::string& /* message */) {
    if (state_ != TransportState::Connected) {
        return false;
    }

    // Stub: Send UDP datagram
    return true;
}

void UdpTransport::set_message_callback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void UdpTransport::set_state_callback(StateCallback callback) {
    state_callback_ = std::move(callback);
}

TransportState UdpTransport::state() const {
    return state_;
}

bool UdpTransport::is_connected() const {
    return state_ == TransportState::Connected;
}

// =============================================================================
// Factory
// =============================================================================

std::unique_ptr<ITransport> create_transport(const TransportConfig& config) {
    return std::make_unique<TcpTransport>(config);
}

}  // namespace Sunny::Infrastructure
