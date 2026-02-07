/**
 * @file INTP001A.h
 * @brief Network Transport (TCP/UDP)
 *
 * Component: INTP001A
 * Domain: IN (Infrastructure) | Category: TP (Transport)
 *
 * Provides network transport for communicating with the
 * Ableton Remote Script bridge:
 * - TCP for reliable command/response (4-byte length-prefixed framing)
 * - UDP for low-latency events (datagram boundary = message boundary)
 *
 * Implementation uses POSIX sockets with poll()-based non-blocking I/O.
 * Reconnection with exponential backoff (1s, 2s, 4s, 8s, max 30s).
 *
 * Preconditions: Valid config with host and port
 * Postconditions: is_connected() reflects actual socket state
 * Failure: send() returns false on failure; state transitions to Error
 */

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
    int max_retry_delay_ms{30000};
};

/// Transport state
enum class TransportState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

/// Message received callback
using MessageCallback = std::function<void(const std::string&)>;

/// State change callback
using StateCallback = std::function<void(TransportState, const std::string&)>;

/**
 * @brief Network Transport Interface
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
 * @brief TCP Transport with length-prefixed framing
 *
 * Framing protocol: each message is prefixed with a 4-byte big-endian
 * length header, followed by the message bytes.
 *
 * Background receive thread uses poll() for non-blocking I/O.
 * Thread-safe send queue for concurrent access.
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
    std::atomic<TransportState> state_{TransportState::Disconnected};
    int socket_fd_{-1};

    MessageCallback message_callback_;
    StateCallback state_callback_;
    std::mutex callback_mutex_;

    std::mutex send_mutex_;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;
    std::thread reconnect_thread_;

    void set_state(TransportState new_state, const std::string& message = "");
    bool connect_socket();
    void close_socket();
    void receive_loop();
    void reconnect_loop();
    bool send_raw(const std::string& message);
    bool send_framed(const void* data, std::size_t len);
    bool recv_exact(void* buf, std::size_t len);
};

/**
 * @brief UDP Transport (connectionless, datagram-based)
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
    std::atomic<TransportState> state_{TransportState::Disconnected};
    int socket_fd_{-1};

    MessageCallback message_callback_;
    StateCallback state_callback_;
    std::mutex callback_mutex_;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;

    void set_state(TransportState new_state, const std::string& message = "");
    void receive_loop();
    void close_socket();
};

/**
 * @brief Create default transport (TCP)
 */
[[nodiscard]] std::unique_ptr<ITransport> create_transport(
    const TransportConfig& config = TransportConfig{}
);

}  // namespace Sunny::Infrastructure
