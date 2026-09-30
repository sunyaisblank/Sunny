"""Integration tests for Sunny's native infrastructure functions.

Tests the pybind11 bindings for the Sunny infrastructure layer.
"""

from __future__ import annotations

import pytest


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
        assert isinstance(result.operation_id, str)
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
        """Message draining retains exact note payloads and read-only value semantics."""
        sn = sunny_native_module

        orch = sn.Orchestrator()

        orch.create_progression_clip(0, 0, "C", "major", ["I", "IV"], 4, 4.0)

        # Drain pending messages
        messages = orch.drain_messages()
        assert len(messages) == 2
        create, add_notes = messages
        assert create.type == sn.BridgeMessageType.CreateClip
        assert create.args == ["4.000000"]
        assert create.notes == []
        assert add_notes.type == sn.BridgeMessageType.AddNotes
        assert add_notes.path == "song/tracks/0/clip_slots/0/clip"
        assert add_notes.args == []
        assert add_notes.notes

        first = add_notes.notes[0]
        assert isinstance(first, sn.NoteEvent)
        assert first.pitch == 60
        assert (first.start_time.numerator, first.start_time.denominator) == (0, 1)
        assert (first.duration.numerator, first.duration.denominator) == (9, 20)
        assert first.end_time().to_float() == first.duration.to_float()
        assert first.velocity == 100
        assert first.muted is False
        with pytest.raises(AttributeError):
            first.pitch = 61

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
