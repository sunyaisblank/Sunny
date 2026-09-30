/**
 * @file transport.hpp
 * @brief LOM Bridge Transport Layer
 *
 *
 * Abstract transport interface for LOM bridge communication.
 * Separates the wire protocol from the delivery mechanism,
 * allowing the Ableton compilers to be tested against a recording
 * transport without a live Ableton session.
 *
 * Two concrete implementations:
 * - CommandBuffer:  records commands for testing and offline compilation
 * - TcpTransport:   delivers commands to a Python Remote Script over TCP
 *
 * Ableton compilers program against the abstract LomTransport interface;
 * the caller selects the backend.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>
#include <sunny/infrastructure/ableton/target_snapshot.hpp>
#include <utility>
#include <vector>

namespace sunny::infrastructure {

/// Maximum UTF-8 JSON payload accepted in either bridge frame direction.
inline constexpr std::size_t SUNNY_BRIDGE_MAX_WIRE_PAYLOAD = 16U * 1024U * 1024U;

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
    [[nodiscard]] virtual LomResponse send_notes(const LomPath& clip_path,
                                                 const std::vector<LomNoteData>& notes) = 0;

    /// Check whether the transport is ready
    [[nodiscard]] virtual bool is_connected() const = 0;

    /// True only for transports that record commands without executing them.
    [[nodiscard]] virtual bool records_without_execution() const { return false; }

    /// Establish or restore a connection when the backend supports it.
    [[nodiscard]] virtual bool ensure_connected() { return is_connected(); }

    /// Describe the target. Null means this transport cannot prove a compiler capability;
    /// Ableton compilers reject that state before mutation.
    [[nodiscard]] virtual sunny::core::Result<std::optional<AbletonTargetProfile>>
    target_profile() {
        return std::optional<AbletonTargetProfile>{};
    }

    /// Observe the structural target precondition in one synchronous bridge call.
    [[nodiscard]] virtual sunny::core::Result<std::optional<AbletonTargetSnapshot>>
    target_snapshot() {
        return std::optional<AbletonTargetSnapshot>{};
    }

    /// Count Session scenes so Score deployment can prove clip slot 0 exists.
    [[nodiscard]] virtual sunny::core::Result<std::optional<std::uint32_t>> scene_count() {
        return std::optional<std::uint32_t>{};
    }

    /// Count existing return tracks when the peer can inspect target state.
    [[nodiscard]] virtual sunny::core::Result<std::optional<std::uint32_t>> return_track_count() {
        return std::optional<std::uint32_t>{};
    }

    /// Count devices on one track-like object when target addressing requires it.
    [[nodiscard]] virtual sunny::core::Result<std::optional<std::uint32_t>>
    device_count(const LomPath&) {
        return std::optional<std::uint32_t>{};
    }
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
        std::vector<LomNoteData> notes; ///< Non-empty only for send_notes
    };

    LomResponse send(const LomRequest& request) override;
    LomResponse send_notes(const LomPath& clip_path,
                           const std::vector<LomNoteData>& notes) override;
    [[nodiscard]] bool is_connected() const override { return true; }
    [[nodiscard]] bool records_without_execution() const override { return true; }
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetProfile>>
    target_profile() override {
        return target_profile_;
    }

    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetSnapshot>>
    target_snapshot() override;

    /// Select a modeled target for capability-boundary tests.
    void set_target_profile(std::optional<AbletonTargetProfile> profile) {
        target_profile_ = std::move(profile);
        if (target_snapshot_ && target_profile_)
            target_snapshot_->target_profile = *target_profile_;
    }

    /// Seed the target topology used by two-phase dry-run tests.
    void set_target_snapshot(AbletonTargetSnapshot snapshot);

    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> scene_count() override {
        return scene_count_;
    }

    void set_scene_count(std::uint32_t count) { scene_count_ = count; }

    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> return_track_count() override {
        return return_track_count_;
    }

    void set_return_track_count(std::uint32_t count) { return_track_count_ = count; }

    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>>
    device_count(const LomPath& track_path) override {
        const auto found = device_counts_.find(track_path.to_string());
        return std::optional<std::uint32_t>{found == device_counts_.end() ? 0 : found->second};
    }

    void set_device_count(const LomPath& track_path, std::uint32_t count) {
        device_counts_[track_path.to_string()] = count;
    }

    /// Access recorded commands
    [[nodiscard]] const std::vector<Entry>& entries() const { return entries_; }

    /// Number of recorded commands
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    /// Clear all recorded commands
    void clear() {
        entries_.clear();
        device_counts_.clear();
    }

    /// Find entries matching a request type
    [[nodiscard]] std::vector<const Entry*> find_by_type(LomRequestType type) const;

    /// Find entries whose path starts with the given prefix
    [[nodiscard]] std::vector<const Entry*> find_by_path_prefix(const std::string& prefix) const;

    /// Count entries of a given request type
    [[nodiscard]] std::size_t count_type(LomRequestType type) const;

  private:
    std::vector<Entry> entries_;
    std::optional<AbletonTargetProfile> target_profile_ =
        modeled_target_profile(AbletonVersion{12, 3, 5, "12.3.5 (modeled test target)"});
    std::optional<std::uint32_t> scene_count_ = 1;
    std::optional<std::uint32_t> return_track_count_ = 0;
    std::map<std::string, std::uint32_t> device_counts_;
    std::optional<AbletonTargetSnapshot> target_snapshot_;
};

// =============================================================================
// TCP Transport — live connection to Python Remote Script
// =============================================================================

/// Connection state
enum class ConnectionState : std::uint8_t { Disconnected, Connecting, Connected, Error };

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
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;
    TcpTransport(TcpTransport&&) = delete;
    TcpTransport& operator=(TcpTransport&&) = delete;

    /// Attempt to connect to the Remote Script
    [[nodiscard]] bool connect();

    /// Disconnect from the Remote Script
    void disconnect();

    LomResponse send(const LomRequest& request) override;
    LomResponse send_notes(const LomPath& clip_path,
                           const std::vector<LomNoteData>& notes) override;
    [[nodiscard]] bool is_connected() const override;
    [[nodiscard]] bool ensure_connected() override;
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetProfile>>
    target_profile() override;
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetSnapshot>>
    target_snapshot() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> scene_count() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> return_track_count() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>>
    device_count(const LomPath& track_path) override;

    /// Current connection state
    [[nodiscard]] ConnectionState state() const { return state_; }

    /// Register a callback for state changes
    void on_state_change(std::function<void(ConnectionState)> callback);

  private:
    class SocketHandle;

    /// Send a length-prefixed frame and receive the response
    LomResponse send_and_receive(const std::string& json_payload);

    /// Send exactly n bytes
    bool send_all(const void* data, std::size_t n);

    /// Receive exactly n bytes
    bool recv_all(void* data, std::size_t n);

    /// Transition state and notify callback
    void set_state(ConnectionState new_state);

    TcpConfig config_;
    std::unique_ptr<SocketHandle> socket_;
    ConnectionState state_ = ConnectionState::Disconnected;
    std::function<void(ConnectionState)> state_callback_;
};

} // namespace sunny::infrastructure
