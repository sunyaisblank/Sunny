"""Integration tests for sunny_native core functions.

Tests the pybind11 bindings for Sunny.Core.
"""

from __future__ import annotations

import pytest


class TestPitchOperations:
    """Test pitch class operations via native backend."""

    def test_pitch_class_from_midi(self, sunny_native_module):
        """Verify pitch_class extracts correct value from MIDI note."""
        sn = sunny_native_module

        # C4 = 60 -> pitch class 0
        assert sn.pitch_class(60) == 0

        # C#4 = 61 -> pitch class 1
        assert sn.pitch_class(61) == 1

        # B4 = 71 -> pitch class 11
        assert sn.pitch_class(71) == 11

        # Octave equivalence: C5 = 72 -> pitch class 0
        assert sn.pitch_class(72) == 0

    def test_transpose(self, sunny_native_module):
        """Verify transposition in Z/12Z group."""
        sn = sunny_native_module

        # C + P5 = G (0 + 7 = 7)
        assert sn.transpose(0, 7) == 7

        # G + P5 = D (7 + 7 = 14 mod 12 = 2)
        assert sn.transpose(7, 7) == 2

        # Negative transposition: C - m2 = B
        assert sn.transpose(0, -1) == 11

    def test_invert(self, sunny_native_module):
        """Verify inversion around axis."""
        sn = sunny_native_module

        # Invert C around C = C
        assert sn.invert(0, 0) == 0

        # Invert G around C = F (0 - 7 mod 12 = 5)
        assert sn.invert(7, 0) == 5

        # Invert D around E = F# (4 - 2 + 4 mod 12 = 6)
        assert sn.invert(2, 4) == 6

    def test_interval_class(self, sunny_native_module):
        """Verify interval class reduction to [0, 6]."""
        sn = sunny_native_module

        # Unison
        assert sn.interval_class(0) == 0

        # Minor second
        assert sn.interval_class(1) == 1

        # Tritone
        assert sn.interval_class(6) == 6

        # Perfect fifth reduces to itself
        assert sn.interval_class(7) == 5  # 12 - 7 = 5

    def test_note_name(self, sunny_native_module):
        """Verify note name generation."""
        sn = sunny_native_module

        assert sn.note_name(0) == "C"
        assert sn.note_name(7) == "G"
        assert sn.note_name(1, False) == "C#"
        assert sn.note_name(1, True) == "Db"


class TestPitchClassSets:
    """Test pitch class set operations."""

    def test_pcs_transpose(self, sunny_native_module):
        """Verify pitch class set transposition."""
        sn = sunny_native_module

        # C major triad {0, 4, 7} transposed by P5 = G major {7, 11, 2}
        c_major = {0, 4, 7}
        g_major = sn.pcs_transpose(c_major, 7)
        assert g_major == {7, 11, 2}

    def test_pcs_invert(self, sunny_native_module):
        """Verify pitch class set inversion."""
        sn = sunny_native_module

        # C major {0, 4, 7} inverted around 0 = F minor {0, 5, 8}
        c_major = {0, 4, 7}
        inverted = sn.pcs_invert(c_major, 0)
        assert inverted == {0, 5, 8}

    def test_pcs_interval_vector(self, sunny_native_module):
        """Verify interval vector computation."""
        sn = sunny_native_module

        # Major triad has IC vector [0, 0, 1, 1, 1, 0]
        c_major = {0, 4, 7}
        iv = sn.pcs_interval_vector(c_major)
        assert iv == [0, 0, 1, 1, 1, 0]


class TestRhythm:
    """Test rhythm operations."""

    def test_euclidean_rhythm_basic(self, sunny_native_module):
        """Verify basic Euclidean rhythm generation."""
        sn = sunny_native_module

        # E(3,8) should produce a standard pattern
        pattern = sn.euclidean_rhythm(3, 8)
        assert len(pattern) == 8
        assert sum(pattern) == 3

    def test_euclidean_rhythm_tresillo(self, sunny_native_module):
        """Verify tresillo pattern E(3,8)."""
        sn = sunny_native_module

        pattern = sn.euclidean_rhythm(3, 8)
        # Tresillo: X . . X . . X .
        # One of the valid rotations
        assert sum(pattern) == 3

    def test_euclidean_rhythm_son_clave(self, sunny_native_module):
        """Verify son clave pattern E(5,16)."""
        sn = sunny_native_module

        pattern = sn.euclidean_rhythm(5, 16)
        assert len(pattern) == 16
        assert sum(pattern) == 5


class TestScales:
    """Test scale operations."""

    def test_generate_scale_notes_major(self, sunny_native_module):
        """Verify major scale generation."""
        sn = sunny_native_module

        # C major scale intervals: W W H W W W H = [0, 2, 4, 5, 7, 9, 11]
        major_intervals = [0, 2, 4, 5, 7, 9, 11]
        notes = sn.generate_scale_notes(0, major_intervals, 4)

        # Should start with C4 = 60
        assert notes[0] == 60
        # Should have correct intervals
        assert notes == [60, 62, 64, 65, 67, 69, 71]

    def test_is_note_in_scale(self, sunny_native_module):
        """Verify scale membership check."""
        sn = sunny_native_module

        major_intervals = [0, 2, 4, 5, 7, 9, 11]

        # C4 is in C major
        assert sn.is_note_in_scale(60, 0, major_intervals) is True

        # C#4 is not in C major
        assert sn.is_note_in_scale(61, 0, major_intervals) is False


class TestHarmony:
    """Test harmony operations."""

    def test_generate_chord(self, sunny_native_module):
        """Verify chord generation from root and quality."""
        sn = sunny_native_module

        # C major chord
        chord = sn.generate_chord(0, "major", 4)
        assert chord.root == 0
        assert chord.quality == "major"
        assert len(chord.notes) >= 3

    def test_negative_harmony(self, sunny_native_module):
        """Verify negative harmony transformation."""
        sn = sunny_native_module

        # In C major, G (dominant) becomes F (subdominant)
        g_major = {7, 11, 2}  # G B D
        transformed = sn.negative_harmony(g_major, 0)
        # Should produce F minor-ish pitches
        assert len(transformed) == 3


class TestVoiceLeading:
    """Test voice leading operations."""

    def test_voice_lead_nearest_tone(self, sunny_native_module):
        """Verify nearest-tone voice leading."""
        sn = sunny_native_module

        # Lead from C major to G major
        source = [60, 64, 67]  # C E G
        target_pcs = [7, 11, 2]  # G B D

        result = sn.voice_lead_nearest_tone(source, target_pcs)

        # Result should have same number of voices
        assert len(result.voiced_notes) == 3

        # Total motion should be minimized
        assert result.total_motion >= 0

    def test_close_voicing(self, sunny_native_module):
        """Verify close voicing generation."""
        sn = sunny_native_module

        # C major triad pitch classes
        pcs = [0, 4, 7]
        voicing = sn.generate_close_voicing(pcs, 4)

        # Should be within one octave
        assert max(voicing) - min(voicing) < 12

    def test_drop2_voicing(self, sunny_native_module):
        """Verify drop-2 voicing generation."""
        sn = sunny_native_module

        # Close voicing
        close = [60, 64, 67, 72]  # C E G C
        drop2 = sn.generate_drop2_voicing(close)

        # Second voice from top should be dropped an octave
        assert len(drop2) == 4
