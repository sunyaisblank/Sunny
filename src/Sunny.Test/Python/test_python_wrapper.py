"""Integration tests for Python wrapper layer.

Tests the thin Python wrapper that delegates to sunny_native.
"""

from __future__ import annotations

import pytest


class TestCoreWrapper:
    """Test sunny.core wrapper module."""

    def test_native_availability_check(self):
        """Verify native availability can be checked."""
        from sunny.core import is_native_available

        # Should return bool without error
        result = is_native_available()
        assert isinstance(result, bool)

    def test_pitch_class_function(self):
        """Verify pitch_class wrapper works."""
        from sunny.core import pitch_class

        assert pitch_class(60) == 0
        assert pitch_class(61) == 1
        assert pitch_class(72) == 0

    def test_transpose_function(self):
        """Verify transpose wrapper works."""
        from sunny.core import transpose

        assert transpose(0, 7) == 7
        assert transpose(7, 7) == 2

    def test_invert_function(self):
        """Verify invert wrapper works."""
        from sunny.core import invert

        assert invert(0, 0) == 0
        assert invert(7, 0) == 5

    def test_euclidean_rhythm_function(self):
        """Verify euclidean_rhythm wrapper works."""
        from sunny.core import euclidean_rhythm

        pattern = euclidean_rhythm(3, 8)
        assert len(pattern) == 8
        assert sum(pattern) == 3


class TestTheoryEngine:
    """Test TheoryEngine class."""

    def test_engine_creation(self, theory_engine):
        """Verify TheoryEngine can be created."""
        assert theory_engine is not None

    def test_get_scale_notes_major(self, theory_engine):
        """Verify major scale generation."""
        notes = theory_engine.get_scale_notes("C", "major", 4)

        assert len(notes) == 7
        assert notes[0] == 60  # C4

    def test_get_scale_notes_minor(self, theory_engine):
        """Verify minor scale generation."""
        notes = theory_engine.get_scale_notes("A", "minor", 4)

        assert len(notes) == 7
        assert notes[0] == 69  # A4

    def test_generate_progression(self, theory_engine):
        """Verify chord progression generation."""
        progression = theory_engine.generate_progression(
            root="C",
            scale="major",
            numerals=["I", "IV", "V", "I"],
            octave=4,
        )

        assert len(progression) == 4

        # Each chord should have notes
        for chord in progression:
            assert "notes" in chord
            assert len(chord["notes"]) >= 3

    def test_voice_lead(self, theory_engine):
        """Verify voice leading."""
        source = [60, 64, 67]  # C major
        target_pcs = [7, 11, 2]  # G major pitch classes

        result = theory_engine.voice_lead(source, target_pcs)

        assert len(result) == 3
        # All notes should be valid MIDI
        for note in result:
            assert 0 <= note <= 127


class TestEngineEdgeCases:
    """Test edge cases in TheoryEngine."""

    def test_unknown_scale(self, theory_engine):
        """Verify handling of unknown scale."""
        with pytest.raises(ValueError):
            theory_engine.get_scale_notes("C", "nonexistent_scale", 4)

    def test_invalid_root(self, theory_engine):
        """Verify handling of invalid root note."""
        with pytest.raises(ValueError):
            theory_engine.get_scale_notes("X", "major", 4)

    def test_empty_progression(self, theory_engine):
        """Verify handling of empty numeral list."""
        progression = theory_engine.generate_progression(
            root="C",
            scale="major",
            numerals=[],
            octave=4,
        )

        assert progression == []

    def test_various_scales(self, theory_engine):
        """Verify various scale types work."""
        scales = [
            "major",
            "minor",
            "dorian",
            "phrygian",
            "lydian",
            "mixolydian",
            "locrian",
        ]

        for scale in scales:
            notes = theory_engine.get_scale_notes("C", scale, 4)
            assert len(notes) == 7, f"Scale {scale} should have 7 notes"


class TestNativeFallback:
    """Test fallback behavior when native is unavailable."""

    def test_fallback_pitch_class(self):
        """Verify fallback pitch_class works."""
        from sunny.core import pitch_class

        # Should work regardless of native availability
        assert pitch_class(60) == 0

    def test_fallback_transpose(self):
        """Verify fallback transpose works."""
        from sunny.core import transpose

        # Should work regardless of native availability
        assert transpose(0, 7) == 7

    def test_fallback_euclidean(self):
        """Verify fallback euclidean_rhythm works."""
        from sunny.core import euclidean_rhythm

        # Should work regardless of native availability
        pattern = euclidean_rhythm(3, 8)
        assert len(pattern) == 8
        assert sum(pattern) == 3
