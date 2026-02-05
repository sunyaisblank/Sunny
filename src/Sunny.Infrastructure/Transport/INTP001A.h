/**
 * @file INTP001A.h
 * @brief Network Transport (TCP/UDP)
 *
 * Component: INTP001A
 * Domain: IN (Infrastructure) | Category: TP (Transport)
 *
 * Provides network transport for communicating with the
 * Ableton Remote Script bridge:
 * - TCP for reliable command/response
 * - UDP for low-latency events (optional)
 *
 * Note: This is a minimal stub. Actual network code would use
 * asio or platform-specific APIs.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Sunny::Infrastructure {

/// Connection configuration
struct TransportConfig {
    std::string host{"127.0.0.1"};
    int tcp_port{9001};
    int udp_port{9002};
    int timeout_ms{5000};
    int retry_count{3};
    int retry_delay_ms{1000};
};

/// Transport state
enum class TransportState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

/// Message received callback
using MessageCallback = std::function<void(const std::string&)>;

/// State change callback
using StateCallback = std::function<void(TransportState, const std::string&)>;

/**
 * @brief Network Transport Interface
 *
 * Abstract interface for network transport.
 * Implementations can use TCP, UDP, WebSocket, etc.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool send(const std::string& message) = 0;
    virtual void set_message_callback(MessageCallback callback) = 0;
    virtual void set_state_callback(StateCallback callback) = 0;
    [[nodiscard]] virtual TransportState state() const = 0;
    [[nodiscard]] virtual bool is_connected() const = 0;
};

/**
 * @brief TCP Transport
 *
 * Reliable transport using TCP sockets.
 * Stub implementation - actual code would use networking library.
 */
class TcpTransport : public ITransport {
public:
    explicit TcpTransport(const TransportConfig& config);
    ~TcpTransport() override;

    bool connect() override;
    void disconnect() override;
    bool send(const std::string& message) override;
    void set_message_callback(MessageCallback callback) override;
    void set_state_callback(StateCallback callback) override;
    [[nodiscard]] TransportState state() const override;
    [[nodiscard]] bool is_connected() const override;

    [[nodiscard]] const TransportConfig& config() const { return config_; }

private:
    TransportConfig config_;
    TransportState state_{TransportState::Disconnected};
    MessageCallback message_callback_;
    StateCallback state_callback_;

    void set_state(TransportState new_state, const std::string& message = "");
};

/**
 * @brief UDP Transport
 *
 * Low-latency transport for events.
 * Stub implementation.
 */
class UdpTransport : public ITransport {
public:
    explicit UdpTransport(const TransportConfig& config);
    ~UdpTransport() override;

    bool connect() override;
    void disconnect() override;
    bool send(const std::string& message) override;
    void set_message_callback(MessageCallback callback) override;
    void set_state_callback(StateCallback callback) override;
    [[nodiscard]] TransportState state() const override;
    [[nodiscard]] bool is_connected() const override;

private:
    TransportConfig config_;
    TransportState state_{TransportState::Disconnected};
    MessageCallback message_callback_;
    StateCallback state_callback_;
};

/**
 * @brief Create default transport
 */
[[nodiscard]] std::unique_ptr<ITransport> create_transport(
    const TransportConfig& config = TransportConfig{}
);

}  // namespace Sunny::Infrastructure
