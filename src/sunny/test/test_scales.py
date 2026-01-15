"""Tests for the Scale System.

Component: THSC001A
Domain: TH (Theory) | Category: SC (Scale)

These tests verify:
- Scale definitions and interval patterns
- Scale note generation across all 40+ scales
- Chord quality mappings for each scale degree
- Scale membership validation
- Edge cases and invariants

Test Organisation:
- TestScaleDefinitions: Verify scale data integrity
- TestChurchModes: Seven diatonic modes
- TestMinorScales: Harmonic and melodic minor
- TestPentatonicBlues: Pentatonic and blues scales
- TestSymmetricScales: Octatonic, whole-tone, augmented
- TestExoticScales: Double harmonic, Hungarian, Persian
- TestBebopScales: Jazz bebop scales
- TestModalVariations: Lydian dominant, altered, etc.
- TestScaleSystemOperations: ScaleSystem class methods
- TestScaleInvariants: Mathematical properties
"""

from __future__ import annotations

import pytest

from sunny.theory.scales import (
    ALL_SCALES,
    BEBOP_SCALES,
    BLUES_SCALES,
    CHURCH_MODES,
    EXOTIC_SCALES,
    MINOR_SCALES,
    MODAL_VARIATIONS,
    PENTATONIC_SCALES,
    SYMMETRIC_SCALES,
    Interval,
    ScaleInfo,
    ScaleSystem,
)


@pytest.fixture
def scale_system() -> ScaleSystem:
    """Create a ScaleSystem instance for testing."""
    return ScaleSystem()


# =============================================================================
# Scale Definition Tests
# =============================================================================


class TestScaleDefinitions:
    """Tests for scale definition integrity."""

    def test_all_scales_have_required_fields(self):
        """Every scale must have name, intervals, chord_qualities, and description."""
        for name, scale in ALL_SCALES.items():
            # Arrange: get scale info
            # Act: access fields
            # Assert: all fields exist and are valid
            assert isinstance(scale.name, str), f"{name}: name must be string"
            assert len(scale.name) > 0, f"{name}: name must not be empty"
            assert isinstance(scale.intervals, tuple), f"{name}: intervals must be tuple"
            assert len(scale.intervals) >= 5, f"{name}: scales must have at least 5 notes"
            assert isinstance(scale.chord_qualities, tuple), f"{name}: chord_qualities must be tuple"
            assert isinstance(scale.description, str), f"{name}: description must be string"

    def test_intervals_start_with_unison(self):
        """All scale intervals must start with 0 (unison/root)."""
        for name, scale in ALL_SCALES.items():
            # Arrange: get scale intervals
            # Act: check first interval
            # Assert: first interval is 0
            assert scale.intervals[0] == 0, f"{name}: first interval must be 0 (unison)"

    def test_intervals_are_ascending(self):
        """Scale intervals must be in strictly ascending order."""
        for name, scale in ALL_SCALES.items():
            # Arrange: get intervals
            intervals = scale.intervals
            # Act & Assert: each interval must be greater than previous
            for i in range(1, len(intervals)):
                assert intervals[i] > intervals[i - 1], (
                    f"{name}: intervals must be strictly ascending, "
                    f"but {intervals[i]} <= {intervals[i - 1]} at position {i}"
                )

    def test_intervals_within_octave(self):
        """All intervals must be within one octave (0-11)."""
        for name, scale in ALL_SCALES.items():
            # Arrange: get intervals
            # Act & Assert: all intervals in range
            for interval in scale.intervals:
                assert 0 <= interval <= 11, (
                    f"{name}: interval {interval} outside octave range [0, 11]"
                )

    def test_chord_qualities_match_scale_length(self):
        """Chord qualities tuple must match scale interval count."""
        for name, scale in ALL_SCALES.items():
            # Arrange: get scale info
            # Act: compare lengths
            # Assert: lengths match
            assert len(scale.chord_qualities) == len(scale.intervals), (
                f"{name}: chord_qualities length ({len(scale.chord_qualities)}) "
                f"must match intervals length ({len(scale.intervals)})"
            )

    def test_chord_qualities_are_valid(self):
        """All chord qualities must be from the valid set."""
        valid_qualities = {"maj", "min", "dim", "aug", "dom", "any"}

        for name, scale in ALL_SCALES.items():
            # Arrange: get chord qualities
            # Act & Assert: all qualities are valid
            for i, quality in enumerate(scale.chord_qualities):
                assert quality in valid_qualities, (
                    f"{name}: invalid chord quality '{quality}' at degree {i + 1}"
                )


# =============================================================================
# Church Modes Tests
# =============================================================================


class TestChurchModes:
    """Tests for the seven church modes (diatonic modes)."""

    def test_ionian_intervals(self):
        """Ionian mode should have major scale intervals: W-W-H-W-W-W-H."""
        # Arrange
        ionian = CHURCH_MODES["ionian"]
        # Act
        expected = (0, 2, 4, 5, 7, 9, 11)
        # Assert
        assert ionian.intervals == expected

    def test_dorian_intervals(self):
        """Dorian mode should have minor with raised 6th."""
        # Arrange
        dorian = CHURCH_MODES["dorian"]
        # Act
        expected = (0, 2, 3, 5, 7, 9, 10)
        # Assert
        assert dorian.intervals == expected

    def test_phrygian_intervals(self):
        """Phrygian mode should have b2 (half step from root)."""
        # Arrange
        phrygian = CHURCH_MODES["phrygian"]
        # Act & Assert: second degree is 1 semitone (b2)
        assert phrygian.intervals[1] == 1

    def test_lydian_intervals(self):
        """Lydian mode should have #4 (augmented 4th/tritone)."""
        # Arrange
        lydian = CHURCH_MODES["lydian"]
        # Act & Assert: fourth degree is 6 semitones (#4)
        assert lydian.intervals[3] == 6

    def test_mixolydian_intervals(self):
        """Mixolydian mode should have b7 (minor 7th)."""
        # Arrange
        mixolydian = CHURCH_MODES["mixolydian"]
        # Act & Assert: seventh degree is 10 semitones (b7)
        assert mixolydian.intervals[6] == 10

    def test_aeolian_intervals(self):
        """Aeolian (natural minor) should have standard minor intervals."""
        # Arrange
        aeolian = CHURCH_MODES["aeolian"]
        # Act
        expected = (0, 2, 3, 5, 7, 8, 10)
        # Assert
        assert aeolian.intervals == expected

    def test_locrian_intervals(self):
        """Locrian mode should have b2 and b5."""
        # Arrange
        locrian = CHURCH_MODES["locrian"]
        # Act & Assert
        assert locrian.intervals[1] == 1  # b2
        assert locrian.intervals[4] == 6  # b5 (tritone from root)

    def test_all_church_modes_are_heptatonic(self):
        """All church modes should have exactly 7 notes."""
        for name, scale in CHURCH_MODES.items():
            # Assert
            assert len(scale.intervals) == 7, f"{name} should have 7 notes"


# =============================================================================
# Minor Scales Tests
# =============================================================================


class TestMinorScales:
    """Tests for harmonic and melodic minor scales."""

    def test_harmonic_minor_raised_seventh(self):
        """Harmonic minor should have raised 7th (leading tone)."""
        # Arrange
        harmonic = MINOR_SCALES["harmonic_minor"]
        natural = CHURCH_MODES["aeolian"]
        # Act & Assert: 7th is 11 (major 7th) not 10 (minor 7th)
        assert harmonic.intervals[6] == 11
        assert natural.intervals[6] == 10

    def test_melodic_minor_raised_sixth_and_seventh(self):
        """Melodic minor should have raised 6th and 7th."""
        # Arrange
        melodic = MINOR_SCALES["melodic_minor"]
        # Act & Assert
        assert melodic.intervals[5] == 9   # Major 6th
        assert melodic.intervals[6] == 11  # Major 7th

    def test_harmonic_minor_augmented_second(self):
        """Harmonic minor should have augmented 2nd between b6 and 7."""
        # Arrange
        harmonic = MINOR_SCALES["harmonic_minor"]
        # Act: interval between 6th and 7th degrees
        interval = harmonic.intervals[6] - harmonic.intervals[5]
        # Assert: augmented 2nd = 3 semitones
        assert interval == 3


# =============================================================================
# Pentatonic and Blues Tests
# =============================================================================


class TestPentatonicBlues:
    """Tests for pentatonic and blues scales."""

    def test_major_pentatonic_intervals(self):
        """Major pentatonic should omit 4th and 7th scale degrees."""
        # Arrange
        pentatonic = PENTATONIC_SCALES["major_pentatonic"]
        # Act
        expected = (0, 2, 4, 7, 9)  # 1, 2, 3, 5, 6
        # Assert
        assert pentatonic.intervals == expected

    def test_minor_pentatonic_intervals(self):
        """Minor pentatonic should be standard minor without 2nd and 6th."""
        # Arrange
        pentatonic = PENTATONIC_SCALES["minor_pentatonic"]
        # Act
        expected = (0, 3, 5, 7, 10)  # 1, b3, 4, 5, b7
        # Assert
        assert pentatonic.intervals == expected

    def test_blues_scale_has_blue_note(self):
        """Blues scale should include the blue note (b5/tritone)."""
        # Arrange
        blues = BLUES_SCALES["blues"]
        # Act & Assert: tritone (6 semitones) should be present
        assert 6 in blues.intervals

    def test_blues_scale_is_hexatonic(self):
        """Blues scale should have 6 notes."""
        # Arrange
        blues = BLUES_SCALES["blues"]
        # Assert
        assert len(blues.intervals) == 6


# =============================================================================
# Symmetric Scales Tests
# =============================================================================


class TestSymmetricScales:
    """Tests for symmetric scales (Messiaen modes)."""

    def test_whole_tone_intervals(self):
        """Whole tone scale should have all whole step intervals."""
        # Arrange
        whole_tone = SYMMETRIC_SCALES["whole_tone"]
        # Act
        expected = (0, 2, 4, 6, 8, 10)
        # Assert
        assert whole_tone.intervals == expected

    def test_whole_tone_is_symmetric(self):
        """Whole tone scale should be completely symmetric (all intervals = 2)."""
        # Arrange
        whole_tone = SYMMETRIC_SCALES["whole_tone"]
        # Act & Assert: all adjacent intervals should be 2
        for i in range(1, len(whole_tone.intervals)):
            diff = whole_tone.intervals[i] - whole_tone.intervals[i - 1]
            assert diff == 2, f"Interval {i} should be 2, got {diff}"

    def test_octatonic_hw_alternates_half_whole(self):
        """Octatonic H-W should alternate half-whole steps."""
        # Arrange
        octatonic = SYMMETRIC_SCALES["octatonic_hw"]
        # Act & Assert: alternating 1, 2, 1, 2...
        expected_diffs = [1, 2, 1, 2, 1, 2, 1]
        for i in range(1, len(octatonic.intervals)):
            diff = octatonic.intervals[i] - octatonic.intervals[i - 1]
            expected = expected_diffs[i - 1]
            assert diff == expected, f"Position {i}: expected {expected}, got {diff}"

    def test_octatonic_wh_alternates_whole_half(self):
        """Octatonic W-H should alternate whole-half steps."""
        # Arrange
        octatonic = SYMMETRIC_SCALES["octatonic_wh"]
        # Act & Assert: alternating 2, 1, 2, 1...
        expected_diffs = [2, 1, 2, 1, 2, 1, 2]
        for i in range(1, len(octatonic.intervals)):
            diff = octatonic.intervals[i] - octatonic.intervals[i - 1]
            expected = expected_diffs[i - 1]
            assert diff == expected, f"Position {i}: expected {expected}, got {diff}"

    def test_chromatic_has_all_twelve_tones(self):
        """Chromatic scale should contain all 12 pitch classes."""
        # Arrange
        chromatic = SYMMETRIC_SCALES["chromatic"]
        # Act
        expected = tuple(range(12))
        # Assert
        assert chromatic.intervals == expected


# =============================================================================
# Exotic Scales Tests
# =============================================================================


class TestExoticScales:
    """Tests for exotic and world music scales."""

    def test_double_harmonic_intervals(self):
        """Double harmonic (Byzantine) should have two augmented seconds."""
        # Arrange
        double_harmonic = EXOTIC_SCALES["double_harmonic"]
        # Act: count augmented seconds (3-semitone gaps)
        aug_seconds = 0
        for i in range(1, len(double_harmonic.intervals)):
            if double_harmonic.intervals[i] - double_harmonic.intervals[i - 1] == 3:
                aug_seconds += 1
        # Assert
        assert aug_seconds >= 2, "Double harmonic should have at least 2 augmented seconds"

    def test_phrygian_dominant_has_b2_and_major_third(self):
        """Phrygian dominant should have b2 and major 3rd."""
        # Arrange
        phrygian_dom = EXOTIC_SCALES["phrygian_dominant"]
        # Assert
        assert phrygian_dom.intervals[1] == 1   # b2
        assert phrygian_dom.intervals[2] == 4   # Major 3rd

    def test_hungarian_minor_intervals(self):
        """Hungarian minor should have raised 4th."""
        # Arrange
        hungarian = EXOTIC_SCALES["hungarian_minor"]
        # Assert: 4th degree at 6 semitones (#4)
        assert hungarian.intervals[3] == 6


# =============================================================================
# Bebop Scales Tests
# =============================================================================


class TestBebopScales:
    """Tests for jazz bebop scales."""

    def test_bebop_dominant_is_octatonic(self):
        """Bebop dominant should have 8 notes."""
        # Arrange
        bebop = BEBOP_SCALES["bebop_dominant"]
        # Assert
        assert len(bebop.intervals) == 8

    def test_bebop_dominant_has_both_sevenths(self):
        """Bebop dominant should have both major and minor 7th."""
        # Arrange
        bebop = BEBOP_SCALES["bebop_dominant"]
        # Assert: both 10 (b7) and 11 (7) present
        assert 10 in bebop.intervals
        assert 11 in bebop.intervals

    def test_all_bebop_scales_are_octatonic(self):
        """All bebop scales should have 8 notes for chromatic passing tones."""
        for name, scale in BEBOP_SCALES.items():
            # Assert
            assert len(scale.intervals) == 8, f"{name} should have 8 notes"


# =============================================================================
# Modal Variations Tests
# =============================================================================


class TestModalVariations:
    """Tests for extended modal variations."""

    def test_lydian_dominant_intervals(self):
        """Lydian dominant should have #4 and b7."""
        # Arrange
        lydian_dom = MODAL_VARIATIONS["lydian_dominant"]
        # Assert
        assert lydian_dom.intervals[3] == 6   # #4
        assert lydian_dom.intervals[6] == 10  # b7

    def test_super_locrian_all_alterations(self):
        """Super Locrian (altered) should have all altered tensions."""
        # Arrange
        altered = MODAL_VARIATIONS["super_locrian"]
        # Assert: b2, b3, b4, b5, b6, b7 (all flattened)
        assert altered.intervals[1] == 1  # b9/b2
        assert altered.intervals[2] == 3  # #9/b3


# =============================================================================
# ScaleSystem Operations Tests
# =============================================================================


class TestScaleSystemOperations:
    """Tests for ScaleSystem class methods."""

    def test_get_scale_by_name(self, scale_system):
        """get_scale should return ScaleInfo for valid name."""
        # Arrange & Act
        scale = scale_system.get_scale("dorian")
        # Assert
        assert scale is not None
        assert isinstance(scale, ScaleInfo)
        assert scale.name == "Dorian"

    def test_get_scale_normalizes_name(self, scale_system):
        """get_scale should normalise name variations."""
        # Arrange & Act & Assert
        assert scale_system.get_scale("DORIAN") is not None
        assert scale_system.get_scale("Dorian") is not None
        assert scale_system.get_scale("harmonic-minor") is not None
        assert scale_system.get_scale("harmonic_minor") is not None

    def test_get_scale_returns_none_for_unknown(self, scale_system):
        """get_scale should return None for unknown scale."""
        # Arrange & Act
        result = scale_system.get_scale("nonexistent_scale")
        # Assert
        assert result is None

    def test_list_scales_returns_all(self, scale_system):
        """list_scales should return all unique scale names."""
        # Arrange & Act
        names = scale_system.list_scales()
        # Assert
        assert len(names) >= 30  # At least 30 unique scales
        assert "ionian" in names
        assert "dorian" in names
        assert "harmonic_minor" in names

    def test_get_scale_notes_c_major(self, scale_system):
        """get_scale_notes should return correct MIDI notes for C major."""
        # Arrange
        root_midi = 60  # Middle C
        # Act
        notes = scale_system.get_scale_notes(root_midi, "ionian")
        # Assert: C major scale from C4
        expected = [60, 62, 64, 65, 67, 69, 71]
        assert notes == expected

    def test_get_scale_notes_transposition(self, scale_system):
        """get_scale_notes should transpose correctly to different roots."""
        # Arrange
        c_notes = scale_system.get_scale_notes(60, "ionian")
        d_notes = scale_system.get_scale_notes(62, "ionian")
        # Act: D major should be C major + 2 semitones
        # Assert
        for i in range(len(c_notes)):
            assert d_notes[i] == c_notes[i] + 2

    def test_get_scale_notes_raises_for_unknown(self, scale_system):
        """get_scale_notes should raise ValueError for unknown scale."""
        # Arrange & Act & Assert
        with pytest.raises(ValueError, match="Unknown scale"):
            scale_system.get_scale_notes(60, "invalid_scale")

    def test_get_chord_quality_c_major_degrees(self, scale_system):
        """get_chord_quality should return correct qualities for C major."""
        # Arrange
        expected = ["maj", "min", "min", "maj", "maj", "min", "dim"]
        # Act & Assert
        for degree, expected_quality in enumerate(expected, start=1):
            quality = scale_system.get_chord_quality("ionian", degree)
            assert quality == expected_quality, f"Degree {degree}: expected {expected_quality}, got {quality}"

    def test_get_chord_quality_raises_for_invalid_degree(self, scale_system):
        """get_chord_quality should raise ValueError for invalid degree."""
        # Arrange & Act & Assert
        with pytest.raises(ValueError, match="Invalid degree"):
            scale_system.get_chord_quality("ionian", 0)
        with pytest.raises(ValueError, match="Invalid degree"):
            scale_system.get_chord_quality("ionian", 8)

    def test_is_note_in_scale_c_major(self, scale_system):
        """is_note_in_scale should correctly identify C major scale members."""
        # Arrange
        root = 60  # C4
        in_scale = [60, 62, 64, 65, 67, 69, 71]  # C D E F G A B
        out_of_scale = [61, 63, 66, 68, 70]       # C# Eb F# Ab Bb
        # Act & Assert
        for note in in_scale:
            assert scale_system.is_note_in_scale(note, root, "ionian"), f"Note {note} should be in C major"
        for note in out_of_scale:
            assert not scale_system.is_note_in_scale(note, root, "ionian"), f"Note {note} should not be in C major"

    def test_is_note_in_scale_different_octaves(self, scale_system):
        """is_note_in_scale should work across octaves."""
        # Arrange
        root = 60  # C4
        # Act & Assert: C in different octaves
        assert scale_system.is_note_in_scale(48, root, "ionian")   # C3
        assert scale_system.is_note_in_scale(60, root, "ionian")   # C4
        assert scale_system.is_note_in_scale(72, root, "ionian")   # C5
        assert scale_system.is_note_in_scale(84, root, "ionian")   # C6


# =============================================================================
# Scale Invariants Tests
# =============================================================================


class TestScaleInvariants:
    """Tests for mathematical invariants and properties."""

    def test_invariant_scale_notes_ascending(self, scale_system):
        """Invariant: Scale notes must be strictly ascending."""
        for scale_name in scale_system.list_scales():
            # Arrange & Act
            notes = scale_system.get_scale_notes(60, scale_name)
            # Assert
            for i in range(1, len(notes)):
                assert notes[i] > notes[i - 1], (
                    f"{scale_name}: notes must be ascending, "
                    f"but {notes[i]} <= {notes[i - 1]}"
                )

    def test_invariant_first_note_is_root(self, scale_system):
        """Invariant: First note must equal root MIDI."""
        for root in [48, 60, 72]:  # C3, C4, C5
            for scale_name in scale_system.list_scales():
                # Arrange & Act
                notes = scale_system.get_scale_notes(root, scale_name)
                # Assert
                assert notes[0] == root, f"{scale_name}: first note must be root {root}"

    def test_invariant_notes_within_octave(self, scale_system):
        """Invariant: All notes within one octave of root."""
        for scale_name in scale_system.list_scales():
            # Arrange & Act
            root = 60
            notes = scale_system.get_scale_notes(root, scale_name)
            # Assert
            for note in notes:
                assert root <= note <= root + 11, (
                    f"{scale_name}: note {note} outside octave range "
                    f"[{root}, {root + 11}]"
                )

    def test_invariant_deterministic_output(self, scale_system):
        """Invariant: Same input must produce same output."""
        # Arrange
        root = 60
        scale_name = "dorian"
        # Act: call multiple times
        results = [scale_system.get_scale_notes(root, scale_name) for _ in range(10)]
        # Assert: all results identical
        for result in results[1:]:
            assert result == results[0]

    def test_invariant_all_scales_have_unique_intervals(self):
        """Invariant: Each scale definition should have unique interval pattern."""
        # Arrange: collect all interval patterns (excluding aliases)
        patterns = {}
        for name, scale in ALL_SCALES.items():
            pattern = scale.intervals
            if pattern not in patterns:
                patterns[pattern] = []
            patterns[pattern].append(name)

        # Assert: patterns with multiple names are aliases
        for pattern, names in patterns.items():
            if len(names) > 1:
                # These should be intentional aliases
                # The first one is the "canonical" name
                assert any(name in ["major", "minor", "natural_minor", "diminished_hw",
                                   "diminished_wh", "altered", "spanish_gypsy", "byzantine"]
                          for name in names), f"Unexpected duplicate pattern: {names}"


# =============================================================================
# Alias Tests
# =============================================================================


class TestScaleAliases:
    """Tests for scale name aliases."""

    def test_major_is_ionian(self, scale_system):
        """'major' should be an alias for 'ionian'."""
        assert scale_system.get_scale("major") == scale_system.get_scale("ionian")

    def test_minor_is_aeolian(self, scale_system):
        """'minor' should be an alias for 'aeolian'."""
        assert scale_system.get_scale("minor") == scale_system.get_scale("aeolian")

    def test_natural_minor_is_aeolian(self, scale_system):
        """'natural_minor' should be an alias for 'aeolian'."""
        assert scale_system.get_scale("natural_minor") == scale_system.get_scale("aeolian")

    def test_altered_is_super_locrian(self, scale_system):
        """'altered' should be an alias for 'super_locrian'."""
        assert scale_system.get_scale("altered") == scale_system.get_scale("super_locrian")

    def test_spanish_gypsy_is_phrygian_dominant(self, scale_system):
        """'spanish_gypsy' should be an alias for 'phrygian_dominant'."""
        assert scale_system.get_scale("spanish_gypsy") == scale_system.get_scale("phrygian_dominant")


# =============================================================================
# Edge Cases Tests
# =============================================================================


class TestScaleEdgeCases:
    """Tests for edge cases and boundary conditions."""

    def test_get_scale_notes_low_midi(self, scale_system):
        """get_scale_notes should work for low MIDI values."""
        # Arrange
        root = 0  # Lowest MIDI note
        # Act
        notes = scale_system.get_scale_notes(root, "ionian")
        # Assert
        assert notes[0] == 0
        assert all(n >= 0 for n in notes)

    def test_get_scale_notes_high_midi(self, scale_system):
        """get_scale_notes should work for high MIDI values."""
        # Arrange
        root = 120  # High MIDI note (notes can exceed 127)
        # Act
        notes = scale_system.get_scale_notes(root, "ionian")
        # Assert
        assert notes[0] == 120

    def test_is_note_in_scale_edge_midi_values(self, scale_system):
        """is_note_in_scale should handle edge MIDI values."""
        # Arrange & Act & Assert
        assert scale_system.is_note_in_scale(0, 0, "ionian")
        assert scale_system.is_note_in_scale(127, 60, "ionian")

    def test_empty_scale_name_raises(self, scale_system):
        """Empty scale name should raise or return None."""
        # Arrange & Act
        result = scale_system.get_scale("")
        # Assert
        assert result is None
