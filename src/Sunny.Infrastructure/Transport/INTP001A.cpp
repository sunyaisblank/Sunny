/**
 * @file INTP001A.cpp
 * @brief Network Transport implementation (POSIX sockets)
 *
 * Component: INTP001A
 *
 * TCP: 4-byte big-endian length-prefixed framing, poll()-based receive.
 * UDP: Datagram-based, recvfrom() in background thread.
 * Reconnection: Exponential backoff (1s, 2s, 4s, ..., max 30s).
 */

#include "INTP001A.h"

#include <algorithm>
#include <cstring>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

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
    if (state_.load() == TransportState::Connected) {
        return true;
    }

    set_state(TransportState::Connecting, "Connecting to " +
              config_.host + ":" + std::to_string(config_.tcp_port));

    if (!connect_socket()) {
        set_state(TransportState::Error, "Connection failed");
        return false;
    }

    set_state(TransportState::Connected, "Connected");

    // Start receive thread
    running_.store(true);
    receive_thread_ = std::thread([this]() { receive_loop(); });

    return true;
}

void TcpTransport::disconnect() {
    running_.store(false);

    close_socket();

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }

    set_state(TransportState::Disconnected, "Disconnected");
}

bool TcpTransport::send(const std::string& message) {
    if (state_.load() != TransportState::Connected) {
        return false;
    }

    std::lock_guard<std::mutex> lock(send_mutex_);
    return send_raw(message);
}

void TcpTransport::set_message_callback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = std::move(callback);
}

void TcpTransport::set_state_callback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
}

TransportState TcpTransport::state() const {
    return state_.load();
}

bool TcpTransport::is_connected() const {
    return state_.load() == TransportState::Connected;
}

void TcpTransport::set_state(TransportState new_state, const std::string& message) {
    auto old_state = state_.exchange(new_state);
    if (old_state == new_state) {
        return;
    }

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_callback_) {
        state_callback_(new_state, message);
    }
}

bool TcpTransport::connect_socket() {
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    // Set TCP_NODELAY for low latency
    int flag = 1;
    ::setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Set non-blocking for connect with timeout
    int flags = ::fcntl(socket_fd_, F_GETFL, 0);
    ::fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(config_.tcp_port));
    if (::inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        close_socket();
        return false;
    }

    int ret = ::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr),
                        sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close_socket();
        return false;
    }

    if (ret < 0) {
        // Wait for connection with timeout
        struct pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;

        int poll_ret = ::poll(&pfd, 1, config_.timeout_ms);
        if (poll_ret <= 0) {
            close_socket();
            return false;
        }

        // Check connection result
        int error = 0;
        socklen_t len = sizeof(error);
        ::getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            close_socket();
            return false;
        }
    }

    // Restore blocking mode for send/recv
    ::fcntl(socket_fd_, F_SETFL, flags);

    return true;
}

void TcpTransport::close_socket() {
    if (socket_fd_ >= 0) {
        ::shutdown(socket_fd_, SHUT_RDWR);
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

void TcpTransport::receive_loop() {
    while (running_.load()) {
        if (socket_fd_ < 0) {
            break;
        }

        struct pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100);  // 100ms timeout
        if (ret < 0) {
            if (errno == EINTR) continue;
            set_state(TransportState::Error, "poll() failed");
            break;
        }

        if (ret == 0) continue;  // Timeout

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            set_state(TransportState::Error, "Connection lost");
            // Start reconnection
            reconnect_thread_ = std::thread([this]() { reconnect_loop(); });
            return;
        }

        if (pfd.revents & POLLIN) {
            // Read 4-byte length header
            std::uint32_t length_net = 0;
            if (!recv_exact(&length_net, 4)) {
                set_state(TransportState::Error, "Receive failed");
                reconnect_thread_ = std::thread([this]() { reconnect_loop(); });
                return;
            }

            std::uint32_t length = ntohl(length_net);
            if (length > 1024 * 1024) {  // 1 MB max message
                set_state(TransportState::Error, "Message too large");
                break;
            }

            std::string message(length, '\0');
            if (!recv_exact(message.data(), length)) {
                set_state(TransportState::Error, "Receive failed");
                reconnect_thread_ = std::thread([this]() { reconnect_loop(); });
                return;
            }

            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (message_callback_) {
                message_callback_(message);
            }
        }
    }
}

void TcpTransport::reconnect_loop() {
    set_state(TransportState::Reconnecting, "Reconnecting...");

    int delay_ms = config_.retry_delay_ms;

    while (running_.load()) {
        close_socket();

        // Sleep with backoff
        for (int waited = 0; waited < delay_ms && running_.load(); waited += 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!running_.load()) break;

        if (connect_socket()) {
            set_state(TransportState::Connected, "Reconnected");
            receive_loop();
            return;
        }

        // Exponential backoff
        delay_ms = std::min(delay_ms * 2, config_.max_retry_delay_ms);
    }
}

bool TcpTransport::send_raw(const std::string& message) {
    return send_framed(message.data(), message.size());
}

bool TcpTransport::send_framed(const void* data, std::size_t len) {
    if (socket_fd_ < 0) return false;

    // Send 4-byte length header (big-endian)
    std::uint32_t length_net = htonl(static_cast<std::uint32_t>(len));
    auto sent = ::send(socket_fd_, &length_net, 4, MSG_NOSIGNAL);
    if (sent != 4) {
        set_state(TransportState::Error, "Send failed");
        return false;
    }

    // Send payload
    std::size_t total_sent = 0;
    while (total_sent < len) {
        auto n = ::send(socket_fd_,
                        static_cast<const char*>(data) + total_sent,
                        len - total_sent, MSG_NOSIGNAL);
        if (n <= 0) {
            set_state(TransportState::Error, "Send failed");
            return false;
        }
        total_sent += static_cast<std::size_t>(n);
    }

    return true;
}

bool TcpTransport::recv_exact(void* buf, std::size_t len) {
    std::size_t total_recv = 0;
    while (total_recv < len) {
        auto n = ::recv(socket_fd_,
                        static_cast<char*>(buf) + total_recv,
                        len - total_recv, 0);
        if (n <= 0) {
            return false;
        }
        total_recv += static_cast<std::size_t>(n);
    }
    return true;
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
    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        set_state(TransportState::Error, "Socket creation failed");
        return false;
    }

    // Set default destination via connect()
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(config_.udp_port));
    if (::inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
        close_socket();
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr),
                  sizeof(addr)) < 0) {
        close_socket();
        return false;
    }

    set_state(TransportState::Connected, "UDP ready");

    // Start receive thread
    running_.store(true);
    receive_thread_ = std::thread([this]() { receive_loop(); });

    return true;
}

void UdpTransport::disconnect() {
    running_.store(false);
    close_socket();

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }

    set_state(TransportState::Disconnected, "Disconnected");
}

bool UdpTransport::send(const std::string& message) {
    if (socket_fd_ < 0) {
        return false;
    }

    auto sent = ::send(socket_fd_, message.data(), message.size(), MSG_NOSIGNAL);
    return sent >= 0 && static_cast<std::size_t>(sent) == message.size();
}

void UdpTransport::set_message_callback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    message_callback_ = std::move(callback);
}

void UdpTransport::set_state_callback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
}

TransportState UdpTransport::state() const {
    return state_.load();
}

bool UdpTransport::is_connected() const {
    return state_.load() == TransportState::Connected;
}

void UdpTransport::set_state(TransportState new_state, const std::string& message) {
    auto old_state = state_.exchange(new_state);
    if (old_state == new_state) return;

    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_callback_) {
        state_callback_(new_state, message);
    }
}

void UdpTransport::receive_loop() {
    std::array<char, 65536> buf{};

    while (running_.load()) {
        struct pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        if (pfd.revents & POLLIN) {
            auto n = ::recvfrom(socket_fd_, buf.data(), buf.size(), 0,
                                nullptr, nullptr);
            if (n > 0) {
                std::string message(buf.data(), static_cast<std::size_t>(n));
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (message_callback_) {
                    message_callback_(message);
                }
            }
        }
    }
}

void UdpTransport::close_socket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

// =============================================================================
// Factory
// =============================================================================

std::unique_ptr<ITransport> create_transport(const TransportConfig& config) {
    return std::make_unique<TcpTransport>(config);
}

}  // namespace Sunny::Infrastructure
