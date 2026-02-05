"""Integration tests for sunny_native infrastructure functions.

Tests the pybind11 bindings for Sunny.Infrastructure.
"""

from __future__ import annotations

import pytest


class TestSessionStateMachine:
    """Test session state machine."""

    def test_initial_state(self, sunny_native_module):
        """Verify initial state is disconnected."""
        sn = sunny_native_module

        session = sn.SessionStateMachine()

        assert session.connection_state() == sn.ConnectionState.Disconnected
        assert session.mode() == sn.SessionMode.Idle
        assert not session.is_connected()

    def test_connection_transitions(self, sunny_native_module):
        """Verify connection state transitions."""
        sn = sunny_native_module

        session = sn.SessionStateMachine()

        # Connecting
        session.set_connecting()
        assert session.connection_state() == sn.ConnectionState.Connecting
        assert session.connection_state_string() == "connecting"

        # Connected
        session.set_connected()
        assert session.connection_state() == sn.ConnectionState.Connected
        assert session.is_connected()
        assert session.connection_state_string() == "connected"

        # Disconnected
        session.set_disconnected("Test disconnect")
        assert session.connection_state() == sn.ConnectionState.Disconnected
        assert not session.is_connected()

    def test_error_state(self, sunny_native_module):
        """Verify error state handling."""
        sn = sunny_native_module

        session = sn.SessionStateMachine()
        session.set_connected()

        session.set_error("Connection lost")
        assert session.connection_state() == sn.ConnectionState.Error
        assert not session.is_connected()

    def test_mode_transitions(self, sunny_native_module):
        """Verify session mode transitions."""
        sn = sunny_native_module

        session = sn.SessionStateMachine()

        # Playing
        session.start_playing()
        assert session.mode() == sn.SessionMode.Playing
        assert session.mode_string() == "playing"

        # Recording
        session.start_recording()
        assert session.mode() == sn.SessionMode.Recording
        assert session.mode_string() == "recording"

        # Stop recording goes to playing
        session.stop_recording()
        assert session.mode() == sn.SessionMode.Playing

        # Stop playing goes to idle
        session.stop_playing()
        assert session.mode() == sn.SessionMode.Idle

    def test_set_mode_directly(self, sunny_native_module):
        """Verify direct mode setting."""
        sn = sunny_native_module

        session = sn.SessionStateMachine()

        session.set_mode(sn.SessionMode.Overdubbing)
        assert session.mode() == sn.SessionMode.Overdubbing


class TestOrchestrator:
    """Test operation orchestrator."""

    def test_orchestrator_creation(self, sunny_native_module):
        """Verify orchestrator can be created."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        assert not orch.can_undo()
        assert not orch.can_redo()
        assert orch.pending_message_count() == 0

    def test_create_progression_clip(self, sunny_native_module):
        """Verify progression clip creation."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        result = orch.create_progression_clip(
            track_index=0,
            slot_index=0,
            root="C",
            scale="major",
            numerals=["I", "IV", "V", "I"],
            octave=4,
            duration_beats=4.0,
        )

        assert result.success
        assert result.operation_id != ""
        assert "progression" in result.message.lower()

        # Should be able to undo
        assert orch.can_undo()

    def test_apply_euclidean_rhythm(self, sunny_native_module):
        """Verify Euclidean rhythm application."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        result = orch.apply_euclidean_rhythm(
            track_index=0,
            slot_index=0,
            pulses=3,
            steps=8,
            pitch=60,
            step_duration=0.25,
        )

        assert result.success
        assert "Euclidean" in result.message

    def test_undo_redo(self, sunny_native_module):
        """Verify undo/redo functionality."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        # Create operation
        orch.create_progression_clip(0, 0, "C", "major", ["I"], 4, 4.0)

        assert orch.can_undo()
        assert not orch.can_redo()

        # Undo
        assert orch.undo()
        assert not orch.can_undo()
        assert orch.can_redo()

        # Redo
        assert orch.redo()
        assert orch.can_undo()
        assert not orch.can_redo()

    def test_clear_history(self, sunny_native_module):
        """Verify history clearing."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        orch.create_progression_clip(0, 0, "C", "major", ["I"], 4, 4.0)
        assert orch.can_undo()

        orch.clear_history()
        assert not orch.can_undo()
        assert not orch.can_redo()

    def test_drain_messages(self, sunny_native_module):
        """Verify message draining."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        orch.create_progression_clip(0, 0, "C", "major", ["I", "IV"], 4, 4.0)

        # Drain pending messages
        messages = orch.drain_messages()
        assert len(messages) > 0

        # After draining, should be empty
        assert orch.pending_message_count() == 0

    def test_max_undo_levels(self, sunny_native_module):
        """Verify max undo levels setting."""
        sn = sunny_native_module

        orch = sn.Orchestrator()
        orch.set_max_undo_levels(5)

        # Create more operations than limit
        for i in range(10):
            orch.create_progression_clip(0, i, "C", "major", ["I"], 4, 4.0)

        # Should only be able to undo 5 times
        undo_count = 0
        while orch.can_undo():
            orch.undo()
            undo_count += 1

        assert undo_count <= 5


class TestBridgeMessage:
    """Test bridge message types."""

    def test_message_types_exist(self, sunny_native_module):
        """Verify message types are accessible."""
        sn = sunny_native_module

        assert hasattr(sn, "BridgeMessageType")
        assert hasattr(sn.BridgeMessageType, "GetProperty")
        assert hasattr(sn.BridgeMessageType, "SetProperty")
        assert hasattr(sn.BridgeMessageType, "CallMethod")
        assert hasattr(sn.BridgeMessageType, "CreateClip")
        assert hasattr(sn.BridgeMessageType, "AddNotes")
