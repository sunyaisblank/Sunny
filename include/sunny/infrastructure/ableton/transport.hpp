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
#include <string_view>
#include <sunny/infrastructure/ableton/legacy_authority.hpp>
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

    [[nodiscard]] virtual sunny::core::Result<std::optional<LegacyPlanningAuthority>>
    capture_legacy_authority() {
        if (records_without_execution()) return std::optional<LegacyPlanningAuthority>{};
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }
    [[nodiscard]] virtual sunny::core::Result<void>
    activate_legacy_workflow(const LegacyWorkflowRecipe&) {
        if (records_without_execution()) return {};
        return std::unexpected(sunny::core::ErrorCode::ProtocolError);
    }
    [[nodiscard]] virtual sunny::core::Result<void> finish_legacy_workflow(bool) { return {}; }

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

/// Why the most recent connection attempt failed.
enum class ConnectFailure : std::uint8_t {
    InvalidConfiguration, ///< a configured timeout cannot bound the exchange
    HostUnresolved,       ///< the host name has no stream address
    ResolverUnavailable,  ///< the owned hostname resolver could not run or returned invalid data
    RequestRevoked,       ///< cancellation or the owning request lifetime forbids further work
    Busy,                 ///< the bridge explicitly declined a second active client
    Refused,              ///< every resolved address refused the connection or was unreachable
    TimedOut,             ///< resolution or connection exceeded the shared connect timeout
    SocketError,          ///< a local socket could not be created or configured
};

/// Operator-facing explanation of a connection failure.
[[nodiscard]] std::string_view describe(ConnectFailure failure) noexcept;

/// Deadline by which the Remote Script's main thread must begin a request
/// (LOM_REQUEST_TIMEOUT_SECONDS in remote_script/Sunny/surface.py). A request
/// not begun by then is cancelled in Live and answered with a definite failure.
inline constexpr std::chrono::milliseconds SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE{10000};

/// Default response deadline. Exceeding the scheduling deadline means a
/// request the transport stops waiting for has already begun in Live: it can
/// no longer start later, and its outcome is reported as indeterminate. The
/// margin covers framing, transit and the main-thread hand-off; the cost is
/// that a Live call hung after it began holds the tool call for this long.
inline constexpr std::chrono::milliseconds SUNNY_BRIDGE_RESPONSE_TIMEOUT{20000};
static_assert(SUNNY_BRIDGE_RESPONSE_TIMEOUT >=
                  SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE + std::chrono::seconds{5},
              "the response deadline must outlast the Remote Script scheduling deadline");

/**
 * @brief TCP transport configuration
 *
 * Invariant for the ordering guarantee above: response_timeout exceeds
 * SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE. Shorter values are accepted so
 * tests can exercise expiry, and forfeit only that guarantee.
 */
struct TcpConfig {
    /// Host name or numeric IPv4/IPv6 address, resolved on every connect.
    std::string host = "127.0.0.1";
    std::uint16_t port = 9001;
    /// Bound on one exchange, from sending the request to its complete response.
    std::chrono::milliseconds response_timeout = SUNNY_BRIDGE_RESPONSE_TIMEOUT;
    /// Bound on resolution and connect combined, across every resolved address.
    std::chrono::milliseconds connect_timeout{10000};
    /// POSIX hostname resolution uses an owned Python 3 stdlib subprocess.
    /// Numeric addresses need no helper. No shell or synchronous DNS fallback.
    std::string resolver_executable = "python3";
};

/**
 * @brief TCP transport for live Ableton communication
 *
 * Connects to the Python Remote Script via TCP, serialises
 * LomRequests to JSON, sends them, and deserialises responses.
 *
 * Wire protocol: 4-byte big-endian length prefix + UTF-8 JSON payload.
 * Uses POSIX sockets; compatible with WSL2 connecting to Windows host.
 * Hostname resolution in the supported POSIX container/WSL client requires
 * Python 3 with its socket/JSON standard library; numeric literals bypass it.
 * A native Windows client resolver/socket profile is not implemented here.
 *
 * The connection persists across requests. Before each request the transport
 * checks whether the peer closed the idle connection (a Remote Script reload,
 * for instance) and reconnects; nothing is in flight at that point, so the
 * replacement cannot duplicate a request. Delivery states are exact: NotSent
 * means no complete frame left this process, and SentWithoutValidResponse
 * means the request may have executed in Live. The connection is dropped
 * after the latter, so a late response can never answer a later request.
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

    /// Why the most recent connect() failed; empty after a successful connect.
    [[nodiscard]] std::optional<ConnectFailure> last_connect_failure() const {
        return last_connect_failure_;
    }

    /// Compatibility failure for the current socket; profile/log diagnostics remain readable.
    [[nodiscard]] const std::optional<std::string>& bridge_identity_error() const {
        return bridge_identity_error_;
    }

    /// Register a callback for state changes
    void on_state_change(std::function<void(ConnectionState)> callback);

  private:
    class SocketHandle;
    using Deadline = std::chrono::steady_clock::time_point;

    enum class Receipt : std::uint8_t {
        Complete,
        DeadlineExpired,
        ConnectionClosed,
        RequestRevoked
    };
    enum class IdlePeer : std::uint8_t { Quiet, Closed, UnsolicitedData };

    /// Pre: Connected. Post: Connected over a connection the peer has not closed,
    /// or Error. Called only between exchanges, when nothing is in flight.
    bool replace_connection_if_peer_closed();

    /// Distinguish idle EOF (safe reconnect) from a pre-admission busy reply.
    [[nodiscard]] IdlePeer idle_peer_state() const;

    /// Close the socket after a failed exchange so nothing can arrive on it later.
    void abandon_connection();

    /// Send a length-prefixed frame and receive the response
    LomResponse send_and_receive(const std::string& json_payload,
                                 bool require_identity = true,
                                 bool read_only = false);

    /// Read-only, nonrecursive handshake before an ordinary request on each new socket.
    bool verify_bridge_identity();

    /// Send exactly n bytes before the same deadline used by receive.
    Receipt send_all(const void* data, std::size_t n, Deadline deadline, bool read_only = false);

    /// Receive exactly n bytes before the deadline
    Receipt recv_all(void* data, std::size_t n, Deadline deadline, bool read_only = false);

    /// Transition state and notify callback
    void set_state(ConnectionState new_state);

    TcpConfig config_;
    std::unique_ptr<SocketHandle> socket_;
    ConnectionState state_ = ConnectionState::Disconnected;
    std::optional<ConnectFailure> last_connect_failure_;
    bool bridge_identity_verified_ = false;
    std::optional<std::string> bridge_identity_error_;
    std::function<void(ConnectionState)> state_callback_;
};

} // namespace sunny::infrastructure
