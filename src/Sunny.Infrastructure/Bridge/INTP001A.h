/**
 * @file INTP001A.h
 * @brief LOM Bridge Transport Layer
 *
 * Component: INTP001A
 * Domain: IN (Infrastructure) | Category: BR (Bridge)
 *
 * Abstract transport interface for LOM bridge communication.
 * Separates the protocol (INBR001A) from the delivery mechanism,
 * allowing the Ableton compilers to be tested against a recording
 * transport without a live Ableton session.
 *
 * Two concrete implementations:
 * - CommandBuffer:  records commands for testing and offline compilation
 * - TcpTransport:   delivers commands to a Python Remote Script over TCP
 *
 * The compilers (FMAB001A, FMAB002A, FMAB003A) program against the
 * abstract LomTransport interface; the caller selects the backend.
 */

#pragma once

#include "INBR001A.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Sunny::Infrastructure {

// =============================================================================
// Abstract Transport
// =============================================================================

/**
 * @brief Abstract transport for LOM bridge commands
 *
 * Pre:  transport is in Connected state (or CommandBuffer, which is always ready)
 * Post: command is delivered and acknowledged, or error is returned
 */
class LomTransport {
public:
    virtual ~LomTransport() = default;

    /// Send a request and wait for a response
    [[nodiscard]] virtual LomResponse send(const LomRequest& request) = 0;

    /// Send a batch of notes to a clip
    [[nodiscard]] virtual LomResponse send_notes(
        const LomPath& clip_path,
        const std::vector<LomNoteData>& notes) = 0;

    /// Check whether the transport is ready
    [[nodiscard]] virtual bool is_connected() const = 0;
};

// =============================================================================
// Command Buffer — recording transport for testing and offline compilation
// =============================================================================

/**
 * @brief Records LOM commands without sending them
 *
 * Each command is stored with its arguments for later inspection.
 * Responses are synthesised as successful acknowledgements.
 * Used by unit tests to verify that compilers produce the correct
 * command sequence.
 */
class CommandBuffer final : public LomTransport {
public:
    /// Recorded command entry
    struct Entry {
        LomRequest request;
        std::vector<LomNoteData> notes;  ///< Non-empty only for send_notes
    };

    LomResponse send(const LomRequest& request) override;
    LomResponse send_notes(
        const LomPath& clip_path,
        const std::vector<LomNoteData>& notes) override;
    [[nodiscard]] bool is_connected() const override { return true; }

    /// Access recorded commands
    [[nodiscard]] const std::vector<Entry>& entries() const { return entries_; }

    /// Number of recorded commands
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    /// Clear all recorded commands
    void clear() { entries_.clear(); }

    /// Find entries matching a request type
    [[nodiscard]] std::vector<const Entry*> find_by_type(
        LomRequestType type) const;

    /// Find entries whose path starts with the given prefix
    [[nodiscard]] std::vector<const Entry*> find_by_path_prefix(
        const std::string& prefix) const;

    /// Count entries of a given request type
    [[nodiscard]] std::size_t count_type(LomRequestType type) const;

private:
    std::vector<Entry> entries_;
};

// =============================================================================
// TCP Transport — live connection to Python Remote Script
// =============================================================================

/// Connection state
enum class ConnectionState : std::uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Error
};

/**
 * @brief TCP transport configuration
 */
struct TcpConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 9001;
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds connect_timeout{10000};
};

/**
 * @brief TCP transport for live Ableton communication
 *
 * Connects to the Python Remote Script via TCP, serialises
 * LomRequests to JSON, sends them, and deserialises responses.
 *
 * Wire protocol: 4-byte big-endian length prefix + UTF-8 JSON payload.
 * Uses POSIX sockets; compatible with WSL2 connecting to Windows host.
 */
class TcpTransport final : public LomTransport {
public:
    explicit TcpTransport(const TcpConfig& config = {});
    ~TcpTransport() override;

    /// Attempt to connect to the Remote Script
    [[nodiscard]] bool connect();

    /// Disconnect from the Remote Script
    void disconnect();

    LomResponse send(const LomRequest& request) override;
    LomResponse send_notes(
        const LomPath& clip_path,
        const std::vector<LomNoteData>& notes) override;
    [[nodiscard]] bool is_connected() const override;

    /// Current connection state
    [[nodiscard]] ConnectionState state() const { return state_; }

    /// Register a callback for state changes
    void on_state_change(std::function<void(ConnectionState)> callback);

private:
    /// Send a length-prefixed frame and receive the response
    LomResponse send_and_receive(const std::string& json_payload);

    /// Send exactly n bytes
    bool send_all(const void* data, std::size_t n);

    /// Receive exactly n bytes
    bool recv_all(void* data, std::size_t n);

    /// Transition state and notify callback
    void set_state(ConnectionState new_state);

    TcpConfig config_;
    int socket_fd_ = -1;
    ConnectionState state_ = ConnectionState::Disconnected;
    std::function<void(ConnectionState)> state_callback_;
};

}  // namespace Sunny::Infrastructure
