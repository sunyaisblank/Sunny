/**
 * @file TSSM001A.cpp
 * @brief Tests for INSM001A (Session State Machine)
 *
 * Component: TSSM001A
 * Domain: TS (Test) | Category: SM (Session)
 *
 * Tests connection state transitions, session mode transitions,
 * observer notifications, idempotent transitions, and string conversions.
 */

#include <catch2/catch_test_macros.hpp>

#include "Session/INSM001A.h"

using namespace Sunny::Infrastructure;

// =============================================================================
// Initial State
// =============================================================================

TEST_CASE("TSSM001A: Initial state", "[infrastructure][session]") {
    SessionStateMachine sm;

    REQUIRE(sm.connection_state() == ConnectionState::Disconnected);
    REQUIRE(sm.mode() == SessionMode::Idle);
    REQUIRE_FALSE(sm.is_connected());
}

// =============================================================================
// Connection State Transitions
// =============================================================================

TEST_CASE("TSSM001A: set_connecting", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.set_connecting();
    REQUIRE(sm.connection_state() == ConnectionState::Connecting);
    REQUIRE_FALSE(sm.is_connected());
}

TEST_CASE("TSSM001A: set_connected", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.set_connecting();
    sm.set_connected();
    REQUIRE(sm.connection_state() == ConnectionState::Connected);
    REQUIRE(sm.is_connected());
}

TEST_CASE("TSSM001A: set_disconnected", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.set_connecting();
    sm.set_connected();
    sm.set_disconnected("test");

    REQUIRE(sm.connection_state() == ConnectionState::Disconnected);
    REQUIRE_FALSE(sm.is_connected());
    REQUIRE(sm.mode() == SessionMode::Idle);
}

TEST_CASE("TSSM001A: set_error", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.set_connecting();
    sm.set_error("timeout");

    REQUIRE(sm.connection_state() == ConnectionState::Error);
    REQUIRE_FALSE(sm.is_connected());
    REQUIRE(sm.mode() == SessionMode::Idle);
}

// =============================================================================
// Session Mode Transitions
// =============================================================================

TEST_CASE("TSSM001A: set_mode", "[infrastructure][session]") {
    SessionStateMachine sm;

    sm.set_mode(SessionMode::Playing);
    REQUIRE(sm.mode() == SessionMode::Playing);

    sm.set_mode(SessionMode::Recording);
    REQUIRE(sm.mode() == SessionMode::Recording);

    sm.set_mode(SessionMode::Overdubbing);
    REQUIRE(sm.mode() == SessionMode::Overdubbing);

    sm.set_mode(SessionMode::Idle);
    REQUIRE(sm.mode() == SessionMode::Idle);
}

TEST_CASE("TSSM001A: start_playing / stop_playing", "[infrastructure][session]") {
    SessionStateMachine sm;

    sm.start_playing();
    REQUIRE(sm.mode() == SessionMode::Playing);

    sm.stop_playing();
    REQUIRE(sm.mode() == SessionMode::Idle);
}

TEST_CASE("TSSM001A: start_recording / stop_recording", "[infrastructure][session]") {
    SessionStateMachine sm;

    sm.start_recording();
    REQUIRE(sm.mode() == SessionMode::Recording);

    sm.stop_recording();
    REQUIRE(sm.mode() == SessionMode::Playing);
}

// =============================================================================
// Observer Notifications
// =============================================================================

TEST_CASE("TSSM001A: Observer receives correct state change", "[infrastructure][session]") {
    SessionStateMachine sm;

    SessionStateChange received{};
    bool called = false;

    sm.add_observer([&](const SessionStateChange& change) {
        received = change;
        called = true;
    });

    sm.set_connecting();
    REQUIRE(called);
    REQUIRE(received.old_connection == ConnectionState::Disconnected);
    REQUIRE(received.new_connection == ConnectionState::Connecting);
}

TEST_CASE("TSSM001A: Observer receives mode change", "[infrastructure][session]") {
    SessionStateMachine sm;

    SessionStateChange received{};
    sm.add_observer([&](const SessionStateChange& change) {
        received = change;
    });

    sm.start_playing();
    REQUIRE(received.old_mode == SessionMode::Idle);
    REQUIRE(received.new_mode == SessionMode::Playing);
}

// =============================================================================
// Idempotent Transitions
// =============================================================================

TEST_CASE("TSSM001A: Idempotent transitions do not fire observers", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.set_connecting();
    sm.set_connected();

    int call_count = 0;
    sm.add_observer([&](const SessionStateChange&) {
        call_count++;
    });

    // set_connected again should be idempotent
    sm.set_connected();
    REQUIRE(call_count == 0);

    // set_disconnected from Disconnected should be idempotent
    sm.set_disconnected();
    call_count = 0;
    sm.add_observer([&](const SessionStateChange&) {
        call_count++;
    });
    sm.set_disconnected();
    REQUIRE(call_count == 0);
}

TEST_CASE("TSSM001A: Idempotent mode transition", "[infrastructure][session]") {
    SessionStateMachine sm;
    sm.start_playing();

    int call_count = 0;
    sm.add_observer([&](const SessionStateChange&) {
        call_count++;
    });

    sm.set_mode(SessionMode::Playing);
    REQUIRE(call_count == 0);
}

// =============================================================================
// String Conversions
// =============================================================================

TEST_CASE("TSSM001A: to_string(ConnectionState)", "[infrastructure][session]") {
    REQUIRE(to_string(ConnectionState::Disconnected) == "disconnected");
    REQUIRE(to_string(ConnectionState::Connecting) == "connecting");
    REQUIRE(to_string(ConnectionState::Connected) == "connected");
    REQUIRE(to_string(ConnectionState::Reconnecting) == "reconnecting");
    REQUIRE(to_string(ConnectionState::Error) == "error");
}

TEST_CASE("TSSM001A: to_string(SessionMode)", "[infrastructure][session]") {
    REQUIRE(to_string(SessionMode::Idle) == "idle");
    REQUIRE(to_string(SessionMode::Playing) == "playing");
    REQUIRE(to_string(SessionMode::Recording) == "recording");
    REQUIRE(to_string(SessionMode::Overdubbing) == "overdubbing");
}

TEST_CASE("TSSM001A: connection_state_string and mode_string", "[infrastructure][session]") {
    SessionStateMachine sm;
    REQUIRE(sm.connection_state_string() == "disconnected");
    REQUIRE(sm.mode_string() == "idle");

    sm.set_connecting();
    REQUIRE(sm.connection_state_string() == "connecting");

    sm.start_playing();
    REQUIRE(sm.mode_string() == "playing");
}

TEST_CASE("TSSM001A: clear_observers", "[infrastructure][session]") {
    SessionStateMachine sm;

    int call_count = 0;
    sm.add_observer([&](const SessionStateChange&) { call_count++; });

    sm.set_connecting();
    REQUIRE(call_count == 1);

    sm.clear_observers();
    sm.set_connected();
    REQUIRE(call_count == 1);  // No increment after clearing
}
