/**
 * @file INSM001A.cpp
 * @brief Session State Machine implementation
 *
 * Component: INSM001A
 */

#include "INSM001A.h"

namespace Sunny::Infrastructure {

SessionStateMachine::SessionStateMachine() = default;

void SessionStateMachine::set_connected() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_state_ == ConnectionState::Connected) {
        return;
    }

    SessionStateChange change;
    change.old_connection = connection_state_;
    change.new_connection = ConnectionState::Connected;
    change.old_mode = mode_;
    change.new_mode = mode_;
    change.message = "Connected to Ableton Live";

    connection_state_ = ConnectionState::Connected;
    notify_observers(change);
}

void SessionStateMachine::set_disconnected(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_state_ == ConnectionState::Disconnected) {
        return;
    }

    SessionStateChange change;
    change.old_connection = connection_state_;
    change.new_connection = ConnectionState::Disconnected;
    change.old_mode = mode_;
    change.new_mode = SessionMode::Idle;
    change.message = reason.empty() ? "Disconnected" : reason;

    connection_state_ = ConnectionState::Disconnected;
    mode_ = SessionMode::Idle;
    notify_observers(change);
}

void SessionStateMachine::set_connecting() {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionStateChange change;
    change.old_connection = connection_state_;
    change.new_connection = ConnectionState::Connecting;
    change.old_mode = mode_;
    change.new_mode = mode_;
    change.message = "Connecting...";

    connection_state_ = ConnectionState::Connecting;
    notify_observers(change);
}

void SessionStateMachine::set_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionStateChange change;
    change.old_connection = connection_state_;
    change.new_connection = ConnectionState::Error;
    change.old_mode = mode_;
    change.new_mode = SessionMode::Idle;
    change.message = error;

    connection_state_ = ConnectionState::Error;
    mode_ = SessionMode::Idle;
    notify_observers(change);
}

ConnectionState SessionStateMachine::connection_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_state_;
}

bool SessionStateMachine::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_state_ == ConnectionState::Connected;
}

void SessionStateMachine::set_mode(SessionMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (mode_ == mode) {
        return;
    }

    SessionStateChange change;
    change.old_connection = connection_state_;
    change.new_connection = connection_state_;
    change.old_mode = mode_;
    change.new_mode = mode;
    change.message = "Mode changed to " + std::string(to_string(mode));

    mode_ = mode;
    notify_observers(change);
}

SessionMode SessionStateMachine::mode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mode_;
}

void SessionStateMachine::start_playing() {
    set_mode(SessionMode::Playing);
}

void SessionStateMachine::stop_playing() {
    set_mode(SessionMode::Idle);
}

void SessionStateMachine::start_recording() {
    set_mode(SessionMode::Recording);
}

void SessionStateMachine::stop_recording() {
    set_mode(SessionMode::Playing);
}

void SessionStateMachine::add_observer(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(std::move(callback));
}

void SessionStateMachine::clear_observers() {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.clear();
}

std::string SessionStateMachine::connection_state_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::string(to_string(connection_state_));
}

std::string SessionStateMachine::mode_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::string(to_string(mode_));
}

void SessionStateMachine::notify_observers(const SessionStateChange& change) {
    for (const auto& callback : observers_) {
        if (callback) {
            callback(change);
        }
    }
}

}  // namespace Sunny::Infrastructure
