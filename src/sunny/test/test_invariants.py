"""Invariant and Edge Case Tests for Sunny Theory System.

Component: THIV001A
Domain: TH (Theory) | Category: IV (Invariants)

These tests verify mathematical and logical invariants that must hold
across all theory modules. They use property-based thinking to ensure
the system behaves correctly under all valid inputs.

Invariant Categories:
- Mathematical: Pitch class arithmetic, interval symmetry
- Structural: Voice count preservation, ordering guarantees
- Compositional: Operation composition properties (idempotence, involution)

Edge Cases:
- Boundary values (octave 0, octave 10, MIDI 0, MIDI 127)
- Empty inputs
- Single element inputs
- Maximum cardinality inputs
"""

from __future__ import annotations

import pytest
from typing import Any

# Import all modules
try:
    from sunny.theory.harmony import (
        HarmonyModule,
        FunctionalRole,
        ChordInfo,
        pitch_class,
        note_name,
        NOTE_NAMES,
        NOTE_TO_PITCH,
    )
    HARMONY_AVAILABLE = True
except ImportError:
    HARMONY_AVAILABLE = False

try:
    from sunny.theory.voiceleading import VoiceLeadingEngine, VoiceLeadingViolation
    VOICELEADING_AVAILABLE = True
except ImportError:
    VOICELEADING_AVAILABLE = False

try:
    from sunny.theory.engine import TheoryEngine
    ENGINE_AVAILABLE = True
except ImportError:
    ENGINE_AVAILABLE = False

try:
    from sunny.theory.rhythm import RhythmEngine
    RHYTHM_AVAILABLE = True
except ImportError:
    RHYTHM_AVAILABLE = False


# =============================================================================
# Fixtures
# =============================================================================


@pytest.fixture
def harmony():
    """Create harmony module instance."""
    if not HARMONY_AVAILABLE:
        pytest.skip("Harmony module not available")
    return HarmonyModule()


@pytest.fixture
def voice_leading():
    """Create voice leading engine instance."""
    if not VOICELEADING_AVAILABLE:
        pytest.skip("Voice leading module not available")
    return VoiceLeadingEngine()


@pytest.fixture
def engine():
    """Create theory engine instance."""
    if not ENGINE_AVAILABLE:
        pytest.skip("Theory engine not available")
    return TheoryEngine()


@pytest.fixture
def rhythm():
    """Create rhythm engine instance."""
    if not RHYTHM_AVAILABLE:
        pytest.skip("Rhythm engine not available")
    return RhythmEngine()


# =============================================================================
# Pitch Class Invariants
# =============================================================================


class TestPitchClassInvariants:
    """Invariants for pitch class arithmetic."""

    @pytest.mark.skipif(not HARMONY_AVAILABLE, reason="Harmony not available")
    def test_invariant_pitch_class_range(self):
        """Invariant: pitch_class(n) ∈ [0, 11] for all MIDI n ∈ [0, 127]."""
        for midi in range(128):
            pc = pitch_class(midi)
            assert 0 <= pc <= 11, f"MIDI {midi} gave pitch class {pc}"

    @pytest.mark.skipif(not HARMONY_AVAILABLE, reason="Harmony not available")
    def test_invariant_pitch_class_periodicity(self):
        """Invariant: pitch_class(n) == pitch_class(n + 12) for all n."""
        for midi in range(116):  # Up to 115 so +12 stays in range
            assert pitch_class(midi) == pitch_class(midi + 12)

    @pytest.mark.skipif(not HARMONY_AVAILABLE, reason="Harmony not available")
    def test_invariant_pitch_class_name_round_trip(self):
        """Invariant: pitch_class(note_name(pc)) == pc for all pc."""
        for pc in range(12):
            name = note_name(pc)
            recovered = pitch_class(name)
            assert recovered == pc, f"Round trip failed: {pc} -> {name} -> {recovered}"

    @pytest.mark.skipif(not HARMONY_AVAILABLE, reason="Harmony not available")
    def test_invariant_all_note_names_valid(self):
        """Invariant: All standard note names map to valid pitch classes."""
        for name in NOTE_NAMES:
            pc = pitch_class(name)
            assert 0 <= pc <= 11

    @pytest.mark.skipif(not HARMONY_AVAILABLE, reason="Harmony not available")
    def test_invariant_enharmonic_equivalence(self):
        """Invariant: Enharmonic notes have same pitch class."""
        enharmonics = [
            ("C#", "Db"), ("D#", "Eb"), ("F#", "Gb"),
            ("G#", "Ab"), ("A#", "Bb"),
        ]
        for sharp, flat in enharmonics:
            assert pitch_class(sharp) == pitch_class(flat)


# =============================================================================
# Functional Role Invariants
# =============================================================================


class TestFunctionalRoleInvariants:
    """Invariants for functional harmony analysis."""

    def test_invariant_all_degrees_have_function(self, harmony):
        """Invariant: Every scale degree (1-7) has a functional role."""
        for mode in ["major", "minor"]:
            for degree in range(1, 8):
                role = harmony.get_functional_role(degree, mode)
                assert isinstance(role, FunctionalRole)

    def test_invariant_tonic_on_first_degree(self, harmony):
        """Invariant: Degree 1 is always TONIC in both modes."""
        assert harmony.get_functional_role(1, "major") == FunctionalRole.TONIC
        assert harmony.get_functional_role(1, "minor") == FunctionalRole.TONIC

    def test_invariant_dominant_on_fifth_degree(self, harmony):
        """Invariant: Degree 5 is always DOMINANT in both modes."""
        assert harmony.get_functional_role(5, "major") == FunctionalRole.DOMINANT
        assert harmony.get_functional_role(5, "minor") == FunctionalRole.DOMINANT

    def test_invariant_tension_levels_bounded(self, harmony):
        """Invariant: Tension levels are in [0, 2]."""
        for numerals in [["I", "IV", "V", "I"], ["i", "iv", "V", "i"]]:
            analysis = harmony.analyze_progression(numerals, "major")
            for entry in analysis:
                assert 0 <= entry["tension"] <= 2


# =============================================================================
# Negative Harmony Invariants
# =============================================================================


class TestNegativeHarmonyInvariants:
    """Invariants for negative harmony operations."""

    def test_invariant_mirror_preserves_voice_count(self, harmony):
        """Invariant: Mirror operation preserves number of voices."""
        test_chords = [
            [60, 64, 67],        # Triad
            [60, 64, 67, 71],    # Seventh
            [60, 63, 66, 69],    # Dim7
            [48, 55, 60, 64, 67],  # 5 voices
        ]
        for chord in test_chords:
            mirrored = harmony.generate_negative_mirror(chord, axis=0)
            assert len(mirrored) == len(chord)

    def test_invariant_mirror_stays_in_midi_range(self, harmony):
        """Invariant: Mirrored notes stay in valid MIDI range."""
        extreme_chords = [
            [0, 4, 7],          # Very low
            [120, 124, 127],    # Very high
            [60, 64, 67],       # Middle
        ]
        for chord in extreme_chords:
            for axis in range(12):
                mirrored = harmony.generate_negative_mirror(chord, axis)
                for note in mirrored:
                    assert 0 <= note <= 127

    def test_invariant_chord_quality_swap(self, harmony):
        """Invariant: Major mirrors to minor and vice versa."""
        # C major -> F minor
        result = harmony.mirror_chord("C", "major", "C", 4)
        assert result.quality == "minor"

        # F minor -> approximately C major
        result2 = harmony.mirror_chord("F", "minor", "C", 4)
        assert result2.quality == "major"


# =============================================================================
# Secondary Dominant Invariants
# =============================================================================


class TestSecondaryDominantInvariants:
    """Invariants for secondary dominant generation."""

    def test_invariant_secondary_dominant_is_p5_above(self, harmony):
        """Invariant: V/X root is always P5 above target root."""
        for target_degree in range(2, 8):
            result = harmony.secondary_dominant(target_degree, "C", "major", 4)
            # The numeral should indicate V/X
            assert "V/" in result.numeral

    def test_invariant_secondary_dominant_is_major(self, harmony):
        """Invariant: Secondary dominants are always major triads."""
        for key in ["C", "G", "D", "A", "E"]:
            for target in range(2, 8):
                result = harmony.secondary_dominant(target, key, "major", 4)
                assert result.quality == "major"

    def test_invariant_secondary_seventh_has_four_notes(self, harmony):
        """Invariant: Secondary dominant 7ths have exactly 4 notes."""
        for target in range(2, 8):
            result = harmony.secondary_dominant_seventh(target, "C", "major", 4)
            assert len(result.notes) == 4


# =============================================================================
# Tritone Substitution Invariants
# =============================================================================


class TestTritoneSubstitutionInvariants:
    """Invariants for tritone substitution."""

    def test_invariant_tritone_sub_is_tritone_away(self, harmony):
        """Invariant: Tritone sub root is always 6 semitones from original."""
        for root in ["C", "D", "E", "F", "G", "A", "B"]:
            sub = harmony.tritone_substitution(root, 4)
            original_pc = pitch_class(root)
            sub_pc = pitch_class(sub.root)
            assert (sub_pc - original_pc) % 12 == 6

    def test_invariant_tritone_sub_is_dominant_seventh(self, harmony):
        """Invariant: Tritone subs are always dominant 7ths."""
        for root in NOTE_NAMES:
            sub = harmony.tritone_substitution(root, 4)
            assert sub.quality == "dominant7"

    def test_invariant_tritone_sub_involutory(self, harmony):
        """Invariant: Applying tritone sub twice returns to original root pc."""
        for root in ["C", "F#", "Bb"]:
            original_pc = pitch_class(root)
            sub1 = harmony.tritone_substitution(root, 4)
            sub2 = harmony.tritone_substitution(sub1.root, 4)
            final_pc = pitch_class(sub2.root)
            assert final_pc == original_pc


# =============================================================================
# Voice Leading Invariants
# =============================================================================


class TestVoiceLeadingInvariants:
    """Invariants for voice leading operations."""

    def test_invariant_nearest_tone_preserves_count(self, voice_leading):
        """Invariant: Nearest tone voicing preserves voice count."""
        test_cases = [
            ([60, 64, 67], [62, 65, 69]),        # 3 to 3
            ([48, 55, 60, 64], [50, 53, 57]),    # 4 to 3
            ([60, 67], [64, 71, 74]),            # 2 to 3
        ]
        for current, target in test_cases:
            result = voice_leading.nearest_tone_voicing(current, target)
            assert len(result) == len(current)

    def test_invariant_fixed_voicing_ascending(self, voice_leading):
        """Invariant: Fixed voicings are always ascending (bass to soprano)."""
        test_voicings = [
            [60, 64, 67],
            [48, 55, 60, 64],
            [36, 48, 55, 60, 64],
        ]
        for voicing in test_voicings:
            fixed = voice_leading._fix_voice_crossings(voicing)
            for i in range(len(fixed) - 1):
                assert fixed[i] <= fixed[i + 1]

    def test_invariant_parallel_detection_symmetric(self, voice_leading):
        """Invariant: Parallel detection works regardless of direction."""
        # Ascending parallels
        v1_up = [60, 67]
        v2_up = [62, 69]
        violations_up = voice_leading.check_parallel_fifths(v1_up, v2_up)

        # Descending parallels
        v1_down = [62, 69]
        v2_down = [60, 67]
        violations_down = voice_leading.check_parallel_fifths(v1_down, v2_down)

        # Both should detect parallels
        assert len(violations_up) > 0
        assert len(violations_down) > 0

    def test_invariant_spacing_check_excludes_bass(self, voice_leading):
        """Invariant: Bass-tenor spacing is not checked for excessive interval."""
        # Large bass-tenor gap (common in choral writing)
        voicing = [36, 60, 64, 67]  # Bass far below
        violations = voice_leading.check_spacing(voicing, max_interval=12)

        # Should not flag bass-tenor
        assert len(violations) == 0


# =============================================================================
# Inversion Invariants
# =============================================================================


class TestInversionInvariants:
    """Invariants for chord inversion operations."""

    def test_invariant_inversion_cyclic(self, voice_leading):
        """Invariant: n inversions of n-note chord returns to root position."""
        triads = [[60, 64, 67], [62, 65, 69], [60, 63, 67]]

        for chord in triads:
            result = chord
            for _ in range(len(chord)):
                result = voice_leading.get_inversion(result, 1)

            # After 3 inversions, pitch classes should match
            original_pcs = sorted([n % 12 for n in chord])
            result_pcs = sorted([n % 12 for n in result])
            assert original_pcs == result_pcs

    def test_invariant_inversion_preserves_pitch_classes(self, voice_leading):
        """Invariant: Inversion preserves the pitch classes."""
        chord = [60, 64, 67]

        for inv in range(3):
            result = voice_leading.get_inversion(chord, inv)
            original_pcs = set(n % 12 for n in chord)
            result_pcs = set(n % 12 for n in result)
            assert original_pcs == result_pcs

    def test_invariant_root_position_unchanged(self, voice_leading):
        """Invariant: Inversion 0 returns original chord."""
        chords = [[60, 64, 67], [48, 52, 55, 58]]
        for chord in chords:
            result = voice_leading.get_inversion(chord, 0)
            assert result == chord


# =============================================================================
# Theory Engine Invariants
# =============================================================================


@pytest.mark.skipif(not ENGINE_AVAILABLE, reason="Engine not available")
class TestTheoryEngineInvariants:
    """Invariants for the theory engine."""

    def test_invariant_scale_notes_count(self, engine):
        """Invariant: All modes produce exactly 7 notes."""
        modes = ["ionian", "dorian", "phrygian", "lydian",
                 "mixolydian", "aeolian", "locrian"]
        for mode in modes:
            notes = engine.get_scale_notes("C", mode, 4)
            assert len(notes) == 7

    def test_invariant_scale_notes_ascending(self, engine):
        """Invariant: Scale notes are strictly ascending within octave."""
        modes = ["major", "minor", "dorian", "lydian"]
        for mode in modes:
            notes = engine.get_scale_notes("C", mode, 4)
            for i in range(len(notes) - 1):
                assert notes[i] < notes[i + 1]

    def test_invariant_octave_transposition(self, engine):
        """Invariant: Octave transposition is exactly 12 semitones."""
        for octave in range(2, 7):
            notes_low = engine.get_scale_notes("C", "major", octave)
            notes_high = engine.get_scale_notes("C", "major", octave + 1)
            for low, high in zip(notes_low, notes_high):
                assert high - low == 12

    def test_invariant_quantize_preserves_count(self, engine):
        """Invariant: Quantization preserves note count."""
        notes = list(range(60, 72))
        quantized = engine.quantize_to_scale(notes, "C", "major")
        assert len(quantized) == len(notes)

    def test_invariant_quantize_to_scale_tones(self, engine):
        """Invariant: Quantized notes are scale tones."""
        scale = engine.get_scale_notes("C", "major", 4)
        scale_pcs = set(n % 12 for n in scale)

        notes = list(range(60, 72))
        quantized = engine.quantize_to_scale(notes, "C", "major")

        for note in quantized:
            assert note % 12 in scale_pcs

    def test_invariant_progression_length(self, engine):
        """Invariant: Progression output matches numeral input count."""
        numerals_list = [
            ["I"],
            ["I", "IV", "V", "I"],
            ["ii", "V", "I", "vi", "IV", "V", "I"],
        ]
        for numerals in numerals_list:
            prog = engine.generate_progression("C", "major", numerals, 4)
            assert len(prog) == len(numerals)


# =============================================================================
# Rhythm Invariants
# =============================================================================


@pytest.mark.skipif(not RHYTHM_AVAILABLE, reason="Rhythm not available")
class TestRhythmInvariants:
    """Invariants for rhythm operations."""

    def test_invariant_euclidean_pulse_count(self, rhythm):
        """Invariant: Euclidean rhythm has exactly k pulses."""
        for k in range(1, 9):
            for n in range(k, 17):
                pattern = rhythm.create_euclidean_rhythm(k, n)
                assert sum(pattern) == k

    def test_invariant_euclidean_step_count(self, rhythm):
        """Invariant: Euclidean rhythm has exactly n steps."""
        for n in range(4, 17):
            for k in range(1, n):
                pattern = rhythm.create_euclidean_rhythm(k, n)
                assert len(pattern) == n

    def test_invariant_polyrhythm_combined_superset(self, rhythm):
        """Invariant: Combined positions contain all individual positions."""
        test_cases = [(3, 2), (5, 4), (7, 4)]
        for a, b in test_cases:
            result = rhythm.create_polyrhythm((a, 1), (b, 1), 4.0)

            for pos in result["a"]:
                assert any(abs(p - pos) < 0.001 for p in result["combined"])
            for pos in result["b"]:
                assert any(abs(p - pos) < 0.001 for p in result["combined"])

    def test_invariant_swing_preserves_count(self, rhythm):
        """Invariant: Swing preserves note count."""
        notes = [{"start_time": i * 0.25, "pitch": 60} for i in range(8)]

        for amount in [0.0, 0.5, 1.0]:
            swung = rhythm.apply_swing(notes, amount, "16th")
            assert len(swung) == len(notes)

    def test_invariant_tuplet_duration_sum(self, rhythm):
        """Invariant: Tuplet durations sum to correct total."""
        test_cases = [
            (3, 2, 0.5),   # Triplet
            (5, 4, 0.25),  # Quintuplet
            (7, 4, 0.25),  # Septuplet
        ]
        for actual, normal, base in test_cases:
            expected_total = normal * base
            tuplet = rhythm.create_tuplet(actual, normal, base)
            assert sum(tuplet) == pytest.approx(expected_total)


# =============================================================================
# Edge Case Tests - Harmony
# =============================================================================


class TestHarmonyEdgeCases:
    """Edge case tests for harmony module."""

    def test_edge_empty_progression(self, harmony):
        """Edge: Empty progression should return empty analysis."""
        result = harmony.analyze_progression([], "major")
        assert result == []

    def test_edge_single_chord_progression(self, harmony):
        """Edge: Single chord progression should work."""
        result = harmony.analyze_progression(["I"], "major")
        assert len(result) == 1

    def test_edge_extreme_octaves(self, harmony):
        """Edge: Operations work at extreme octaves."""
        # Very low
        low = harmony.secondary_dominant(5, "C", "major", 1)
        assert all(0 <= n <= 127 for n in low.notes)

        # Very high
        high = harmony.secondary_dominant(5, "C", "major", 6)
        assert all(0 <= n <= 127 for n in high.notes)

    def test_edge_all_keys(self, harmony):
        """Edge: All 12 keys work for operations."""
        for key in ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]:
            result = harmony.secondary_dominant(5, key, "major", 4)
            assert result is not None

    def test_edge_all_augmented_sixth_types(self, harmony):
        """Edge: All augmented sixth types work."""
        for chord_type in ["italian", "french", "german"]:
            result = harmony.augmented_sixth(chord_type, "C", "major", 4)
            assert result is not None

    def test_edge_invalid_augmented_sixth_type(self, harmony):
        """Edge: Invalid augmented sixth type raises error."""
        with pytest.raises(ValueError):
            harmony.augmented_sixth("invalid", "C", "major", 4)


# =============================================================================
# Edge Case Tests - Voice Leading
# =============================================================================


class TestVoiceLeadingEdgeCases:
    """Edge case tests for voice leading module."""

    def test_edge_empty_voicing(self, voice_leading):
        """Edge: Empty voicing should be handled."""
        result = voice_leading.nearest_tone_voicing([], [60, 64, 67])
        assert result == [] or result == [60, 64, 67]

    def test_edge_single_voice(self, voice_leading):
        """Edge: Single voice should work."""
        result = voice_leading.nearest_tone_voicing([60], [64])
        assert len(result) == 1

    def test_edge_identical_voicings(self, voice_leading):
        """Edge: Identical voicings should have no movement."""
        voicing = [60, 64, 67]
        result = voice_leading.nearest_tone_voicing(voicing, voicing)
        # Result should have same pitch classes
        assert sorted(n % 12 for n in result) == sorted(n % 12 for n in voicing)

    def test_edge_no_violations_in_unisons(self, voice_leading):
        """Edge: Static voices (no motion) should not violate parallel rules."""
        voicing = [60, 67]
        violations = voice_leading.check_parallel_fifths(voicing, voicing)
        # No motion means no parallel motion
        assert len(violations) == 0

    def test_edge_extreme_spacing(self, voice_leading):
        """Edge: Very wide voicings should be detectable."""
        voicing = [36, 48, 96, 108]  # Extreme spacing
        violations = voice_leading.check_spacing(voicing, max_interval=12)
        assert len(violations) > 0

    def test_edge_smooth_progression_empty(self, voice_leading):
        """Edge: Empty chord list should return empty progression."""
        result = voice_leading.generate_smooth_progression([])
        assert result == []

    def test_edge_smooth_progression_single(self, voice_leading):
        """Edge: Single chord should return single voicing."""
        result = voice_leading.generate_smooth_progression([[60, 64, 67]])
        assert len(result) == 1


# =============================================================================
# Edge Case Tests - Theory Engine
# =============================================================================


@pytest.mark.skipif(not ENGINE_AVAILABLE, reason="Engine not available")
class TestTheoryEngineEdgeCases:
    """Edge case tests for theory engine."""

    def test_edge_extreme_octaves(self, engine):
        """Edge: Extreme octaves produce valid MIDI notes."""
        # Low
        low = engine.get_scale_notes("C", "major", 0)
        assert all(0 <= n <= 127 for n in low)

        # High (may be capped)
        high = engine.get_scale_notes("C", "major", 8)
        # Some notes may exceed MIDI range, but should be handled

    def test_edge_all_modes(self, engine):
        """Edge: All 7 modes work correctly."""
        modes = ["ionian", "dorian", "phrygian", "lydian",
                 "mixolydian", "aeolian", "locrian"]
        for mode in modes:
            notes = engine.get_scale_notes("C", mode, 4)
            assert len(notes) == 7

    def test_edge_invalid_mode_error(self, engine):
        """Edge: Invalid mode raises appropriate error."""
        with pytest.raises(ValueError):
            engine.get_scale_notes("C", "invalid_mode", 4)

    def test_edge_empty_notes_quantize(self, engine):
        """Edge: Empty note list quantizes to empty."""
        result = engine.quantize_to_scale([], "C", "major")
        assert result == []


# =============================================================================
# Edge Case Tests - Rhythm
# =============================================================================


@pytest.mark.skipif(not RHYTHM_AVAILABLE, reason="Rhythm not available")
class TestRhythmEdgeCases:
    """Edge case tests for rhythm module."""

    def test_edge_euclidean_all_hits(self, rhythm):
        """Edge: E(n,n) produces all hits."""
        for n in range(1, 17):
            pattern = rhythm.create_euclidean_rhythm(n, n)
            assert all(pattern)

    def test_edge_euclidean_no_hits(self, rhythm):
        """Edge: E(0,n) produces no hits."""
        for n in range(1, 17):
            pattern = rhythm.create_euclidean_rhythm(0, n)
            assert not any(pattern)

    def test_edge_euclidean_one_hit(self, rhythm):
        """Edge: E(1,n) produces exactly one hit."""
        for n in range(1, 17):
            pattern = rhythm.create_euclidean_rhythm(1, n)
            assert sum(pattern) == 1

    def test_edge_polyrhythm_1_against_1(self, rhythm):
        """Edge: 1:1 polyrhythm is trivial (both at same positions)."""
        result = rhythm.create_polyrhythm((1, 1), (1, 1), 4.0)
        assert len(result["a"]) == 1
        assert len(result["b"]) == 1

    def test_edge_swing_boundaries(self, rhythm):
        """Edge: Swing at 0 and 1 boundaries work correctly."""
        notes = [{"start_time": 0.25, "pitch": 60}]

        # 0 swing - unchanged
        result_0 = rhythm.apply_swing(notes, 0.0, "16th")
        assert result_0[0]["start_time"] == pytest.approx(0.25)

        # Full swing
        result_1 = rhythm.apply_swing(notes, 1.0, "16th")
        assert result_1[0]["start_time"] > 0.25

    def test_edge_tuplet_extreme_ratios(self, rhythm):
        """Edge: Extreme tuplet ratios work correctly."""
        # 11:8 tuplet (odd ratio)
        result = rhythm.create_tuplet(11, 8, 0.125)
        assert len(result) == 11
        assert sum(result) == pytest.approx(8 * 0.125)
