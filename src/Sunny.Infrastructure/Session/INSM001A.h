/**
 * @file INSM001A.h
 * @brief Session State Machine
 *
 * Component: INSM001A
 * Domain: IN (Infrastructure) | Category: SM (State Machine)
 *
 * Manages session lifecycle and state transitions:
 * - Connection states (disconnected, connecting, connected)
 * - Session modes (idle, recording, playing)
 * - State change notifications
 */

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Sunny::Infrastructure {

/// Connection state
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

/// Session mode
enum class SessionMode {
    Idle,
    Playing,
    Recording,
    Overdubbing
};

/// Session state change event
struct SessionStateChange {
    ConnectionState old_connection;
    ConnectionState new_connection;
    SessionMode old_mode;
    SessionMode new_mode;
    std::string message;
};

/// State change callback
using StateChangeCallback = std::function<void(const SessionStateChange&)>;

/**
 * @brief Session State Machine
 *
 * Manages session state and notifies observers of changes.
 * Thread-safe via internal mutex.
 */
class SessionStateMachine {
public:
    SessionStateMachine();

    // Connection state
    void set_connected();
    void set_disconnected(const std::string& reason = "");
    void set_connecting();
    void set_error(const std::string& error);

    [[nodiscard]] ConnectionState connection_state() const;
    [[nodiscard]] bool is_connected() const;

    // Session mode
    void set_mode(SessionMode mode);
    [[nodiscard]] SessionMode mode() const;

    // Transport control shortcuts
    void start_playing();
    void stop_playing();
    void start_recording();
    void stop_recording();

    // Observers
    void add_observer(StateChangeCallback callback);
    void clear_observers();

    // State queries
    [[nodiscard]] std::string connection_state_string() const;
    [[nodiscard]] std::string mode_string() const;

private:
    ConnectionState connection_state_{ConnectionState::Disconnected};
    SessionMode mode_{SessionMode::Idle};
    std::vector<StateChangeCallback> observers_;
    mutable std::mutex mutex_;

    void notify_observers(const SessionStateChange& change);
};

/// Convert connection state to string
[[nodiscard]] constexpr std::string_view to_string(ConnectionState state) noexcept {
    switch (state) {
        case ConnectionState::Disconnected: return "disconnected";
        case ConnectionState::Connecting: return "connecting";
        case ConnectionState::Connected: return "connected";
        case ConnectionState::Reconnecting: return "reconnecting";
        case ConnectionState::Error: return "error";
    }
    return "unknown";
}

/// Convert session mode to string
[[nodiscard]] constexpr std::string_view to_string(SessionMode mode) noexcept {
    switch (mode) {
        case SessionMode::Idle: return "idle";
        case SessionMode::Playing: return "playing";
        case SessionMode::Recording: return "recording";
        case SessionMode::Overdubbing: return "overdubbing";
    }
    return "unknown";
}

}  // namespace Sunny::Infrastructure
