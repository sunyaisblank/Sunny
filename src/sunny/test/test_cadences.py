"""Tests for the Cadence System.

Component: THCA001A
Domain: TH (Theory) | Category: CA (Cadence)

These tests verify:
- Cadence type definitions and properties
- Cadence generation in all keys
- Cadence detection from numeral sequences
- Chord voicing correctness
- Edge cases and invariants

Test Organisation:
- TestCadenceDefinitions: Verify cadence data integrity
- TestPerfectAuthentic: V → I cadence generation
- TestImperfectAuthentic: V → I6 cadence generation
- TestHalfCadence: I → V cadence generation
- TestPlagalCadence: IV → I cadence generation
- TestDeceptiveCadence: V → vi cadence generation
- TestSpecialisedCadences: Picardy, Neapolitan, Phrygian, Backdoor
- TestCadenceDetection: Numeral pattern detection
- TestCadenceInvariants: Mathematical properties
"""

from __future__ import annotations

import pytest

from sunny.theory.cadences import (
    CADENCE_DEFINITIONS,
    CadenceEngine,
    CadenceInfo,
    CadenceType,
)


@pytest.fixture
def engine() -> CadenceEngine:
    """Create a CadenceEngine instance for testing."""
    return CadenceEngine()


# =============================================================================
# Cadence Definition Tests
# =============================================================================


class TestCadenceDefinitions:
    """Tests for cadence definition integrity."""

    def test_all_cadence_types_have_definitions(self):
        """Every CadenceType enum value must have a definition."""
        for cad_type in CadenceType:
            # Arrange & Act
            definition = CADENCE_DEFINITIONS.get(cad_type)
            # Assert
            assert definition is not None, f"{cad_type.value} has no definition"
            assert isinstance(definition, CadenceInfo)

    def test_all_definitions_have_required_fields(self):
        """Every cadence definition must have all required fields."""
        for cad_type, info in CADENCE_DEFINITIONS.items():
            # Assert: all fields present and valid
            assert info.type == cad_type
            assert isinstance(info.numerals, tuple)
            assert len(info.numerals) >= 2, f"{cad_type.value}: must have at least 2 chords"
            assert isinstance(info.description, str)
            assert len(info.description) > 0
            assert isinstance(info.emotional_quality, str)
            assert len(info.emotional_quality) > 0

    def test_cadence_type_enum_values(self):
        """CadenceType enum should have expected values."""
        expected = {
            "perfect_authentic",
            "imperfect_authentic",
            "half",
            "plagal",
            "deceptive",
            "picardy",
            "neapolitan",
            "phrygian",
            "backdoor",
            "minor_plagal",
        }
        actual = {ct.value for ct in CadenceType}
        assert actual == expected


# =============================================================================
# Perfect Authentic Cadence Tests
# =============================================================================


class TestPerfectAuthentic:
    """Tests for V → I perfect authentic cadence."""

    def test_c_major_perfect_authentic(self, engine):
        """Perfect authentic in C major should be G → C."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "V"
        assert chords[0]["root"] == "G"
        assert chords[0]["quality"] == "major"
        assert chords[1]["numeral"] == "I"
        assert chords[1]["root"] == "C"
        assert chords[1]["quality"] == "major"

    def test_a_minor_perfect_authentic(self, engine):
        """Perfect authentic in A minor should be E → Am."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "A", "minor", 4)
        # Assert
        assert chords[0]["numeral"] == "V"
        assert chords[0]["root"] == "E"
        assert chords[0]["quality"] == "major"  # V is always major
        assert chords[1]["numeral"] == "I"
        assert chords[1]["root"] == "A"
        assert chords[1]["quality"] == "minor"

    def test_v_chord_notes_c_major(self, engine):
        """V chord in C major should contain G, B, D."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major", 4)
        v_notes = chords[0]["notes"]
        # Assert: G4, B4, D5 = 67, 71, 74
        assert 67 in v_notes  # G
        assert 71 in v_notes  # B
        assert 74 in v_notes  # D

    def test_i_chord_notes_c_major(self, engine):
        """I chord in C major should contain C, E, G."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major", 4)
        i_notes = chords[1]["notes"]
        # Assert: C4, E4, G4 = 60, 64, 67
        assert 60 in i_notes  # C
        assert 64 in i_notes  # E
        assert 67 in i_notes  # G

    def test_transposition_to_all_keys(self, engine):
        """Perfect authentic should work in all 12 keys."""
        keys = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
        for key in keys:
            # Arrange & Act
            chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, key, "major", 4)
            # Assert: basic structure is correct
            assert len(chords) == 2
            assert chords[0]["numeral"] == "V"
            assert chords[1]["numeral"] == "I"


# =============================================================================
# Imperfect Authentic Cadence Tests
# =============================================================================


class TestImperfectAuthentic:
    """Tests for V → I6 imperfect authentic cadence."""

    def test_c_major_imperfect_authentic(self, engine):
        """Imperfect authentic in C major should be G → C6."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.IMPERFECT_AUTHENTIC, "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "V"
        assert chords[1]["numeral"] == "I6"

    def test_first_inversion_bass_note(self, engine):
        """I6 chord should have third in bass (first inversion)."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.IMPERFECT_AUTHENTIC, "C", "major", 4)
        i6_notes = sorted(chords[1]["notes"])
        # Assert: In C major I6, E should be the lowest note
        # Notes should be: E, G, C (first inversion)
        # The bass (lowest) should be E = 64 or lower octave equivalent
        lowest = i6_notes[0]
        assert lowest % 12 == 4, f"Bass should be E (pitch class 4), got {lowest % 12}"


# =============================================================================
# Half Cadence Tests
# =============================================================================


class TestHalfCadence:
    """Tests for I → V half cadence."""

    def test_c_major_half_cadence(self, engine):
        """Half cadence in C major should be C → G."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.HALF, "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "I"
        assert chords[0]["root"] == "C"
        assert chords[1]["numeral"] == "V"
        assert chords[1]["root"] == "G"

    def test_half_cadence_ends_on_dominant(self, engine):
        """Half cadence should always end on V (dominant)."""
        for key in ["C", "G", "D", "A", "E"]:
            # Arrange & Act
            chords = engine.create_cadence(CadenceType.HALF, key, "major", 4)
            # Assert
            assert chords[-1]["numeral"] == "V"


# =============================================================================
# Plagal Cadence Tests
# =============================================================================


class TestPlagalCadence:
    """Tests for IV → I plagal (Amen) cadence."""

    def test_c_major_plagal(self, engine):
        """Plagal cadence in C major should be F → C."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PLAGAL, "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "IV"
        assert chords[0]["root"] == "F"
        assert chords[1]["numeral"] == "I"
        assert chords[1]["root"] == "C"

    def test_iv_chord_quality_major_key(self, engine):
        """IV chord in major key should be major."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PLAGAL, "C", "major", 4)
        # Assert
        assert chords[0]["quality"] == "major"

    def test_iv_chord_quality_minor_key(self, engine):
        """iv chord in minor key should be minor."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PLAGAL, "A", "minor", 4)
        # Assert
        assert chords[0]["quality"] == "minor"


# =============================================================================
# Deceptive Cadence Tests
# =============================================================================


class TestDeceptiveCadence:
    """Tests for V → vi deceptive cadence."""

    def test_c_major_deceptive(self, engine):
        """Deceptive cadence in C major should be G → Am."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.DECEPTIVE, "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "V"
        assert chords[0]["root"] == "G"
        assert chords[1]["numeral"] == "vi"
        assert chords[1]["root"] == "A"
        assert chords[1]["quality"] == "minor"

    def test_vi_chord_is_relative_minor(self, engine):
        """vi chord should be minor in major key."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.DECEPTIVE, "C", "major", 4)
        # Assert
        assert chords[1]["quality"] == "minor"

    def test_deceptive_in_minor_resolves_to_major(self, engine):
        """In minor key, vi should resolve to major VI (relative major)."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.DECEPTIVE, "A", "minor", 4)
        # Assert: vi in A minor would resolve to F major (bVI)
        assert chords[1]["quality"] == "major"


# =============================================================================
# Specialised Cadence Tests
# =============================================================================


class TestPicardyCadence:
    """Tests for Picardy third cadence."""

    def test_picardy_ends_major_in_minor_context(self, engine):
        """Picardy cadence should end on major I in minor context."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PICARDY, "A", "minor", 4)
        # Assert: Last chord is major (the Picardy third)
        assert chords[-1]["quality"] == "major"
        assert chords[-1]["numeral"] == "I"

    def test_picardy_note_annotation(self, engine):
        """Picardy cadence should include explanatory note."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PICARDY, "A", "minor", 4)
        # Assert
        assert "note" in chords[-1]
        assert "Picardy" in chords[-1]["note"]


class TestNeapolitanCadence:
    """Tests for Neapolitan cadence (bII6 → V → I)."""

    def test_neapolitan_has_three_chords(self, engine):
        """Neapolitan cadence should have 3 chords."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.NEAPOLITAN, "A", "minor", 4)
        # Assert
        assert len(chords) == 3

    def test_neapolitan_first_chord_is_bii(self, engine):
        """First chord of Neapolitan should be bII6."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.NEAPOLITAN, "A", "minor", 4)
        # Assert
        assert chords[0]["numeral"] == "bII6"

    def test_neapolitan_bii_root_in_a_minor(self, engine):
        """bII in A minor should be Bb major."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.NEAPOLITAN, "A", "minor", 4)
        # Assert
        assert chords[0]["root"] == "Bb"
        assert chords[0]["quality"] == "major"


class TestPhrygianCadence:
    """Tests for Phrygian half cadence (iv6 → V)."""

    def test_phrygian_has_two_chords(self, engine):
        """Phrygian cadence should have 2 chords."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PHRYGIAN, "A", "minor", 4)
        # Assert
        assert len(chords) == 2

    def test_phrygian_ends_on_dominant(self, engine):
        """Phrygian cadence should end on V."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PHRYGIAN, "A", "minor", 4)
        # Assert
        assert chords[-1]["numeral"] == "V"

    def test_phrygian_iv_is_minor_first_inversion(self, engine):
        """First chord of Phrygian should be iv6 (minor, first inversion)."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PHRYGIAN, "A", "minor", 4)
        # Assert
        assert chords[0]["numeral"] == "iv6"
        assert chords[0]["quality"] == "minor"


class TestBackdoorCadence:
    """Tests for backdoor cadence (bVII → I)."""

    def test_backdoor_uses_flat_seven(self, engine):
        """Backdoor cadence should use bVII chord."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.BACKDOOR, "C", "major", 4)
        # Assert
        assert chords[0]["numeral"] == "bVII"
        assert chords[0]["root"] == "Bb"

    def test_backdoor_ends_on_tonic(self, engine):
        """Backdoor cadence should end on I."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.BACKDOOR, "C", "major", 4)
        # Assert
        assert chords[-1]["numeral"] == "I"


class TestMinorPlagalCadence:
    """Tests for minor plagal cadence (iv → I)."""

    def test_minor_plagal_iv_is_minor(self, engine):
        """iv chord in minor plagal should be minor."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.MINOR_PLAGAL, "C", "major", 4)
        # Assert
        assert chords[0]["numeral"] == "iv"
        assert chords[0]["quality"] == "minor"

    def test_minor_plagal_i_is_major(self, engine):
        """I chord in minor plagal should be major."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.MINOR_PLAGAL, "C", "major", 4)
        # Assert
        assert chords[-1]["numeral"] == "I"
        assert chords[-1]["quality"] == "major"


# =============================================================================
# Cadence Detection Tests
# =============================================================================


class TestCadenceDetection:
    """Tests for cadence detection from numeral sequences."""

    def test_detect_perfect_authentic(self, engine):
        """Should detect V → I as perfect authentic."""
        # Arrange & Act
        result = engine.detect_cadence(["V", "I"])
        # Assert
        assert result == CadenceType.PERFECT_AUTHENTIC

    def test_detect_plagal(self, engine):
        """Should detect IV → I as plagal."""
        # Arrange & Act
        result = engine.detect_cadence(["IV", "I"])
        # Assert
        assert result == CadenceType.PLAGAL

    def test_detect_deceptive(self, engine):
        """Should detect V → vi as deceptive."""
        # Arrange & Act
        result = engine.detect_cadence(["V", "vi"])
        # Assert
        assert result == CadenceType.DECEPTIVE

    def test_detect_half_cadence(self, engine):
        """Should detect progression ending on V as half."""
        # Arrange & Act
        result = engine.detect_cadence(["I", "V"])
        # Assert
        assert result == CadenceType.HALF

    def test_detect_neapolitan_three_chords(self, engine):
        """Should detect bII → V → I as Neapolitan."""
        # Arrange & Act
        result = engine.detect_cadence(["bII6", "V", "I"])
        # Assert
        assert result == CadenceType.NEAPOLITAN

    def test_detect_returns_none_for_single_chord(self, engine):
        """Should return None for single chord input."""
        # Arrange & Act
        result = engine.detect_cadence(["I"])
        # Assert
        assert result is None

    def test_detect_handles_seventh_chords(self, engine):
        """Should detect cadences with seventh chord numerals."""
        # Arrange & Act
        result = engine.detect_cadence(["V7", "I"])
        # Assert
        assert result == CadenceType.PERFECT_AUTHENTIC

    def test_detect_case_insensitive(self, engine):
        """Detection should be case-insensitive."""
        # Arrange & Act
        result = engine.detect_cadence(["v", "i"])
        # Assert
        assert result == CadenceType.PERFECT_AUTHENTIC


# =============================================================================
# Cadence Info Tests
# =============================================================================


class TestCadenceInfo:
    """Tests for get_cadence_info method."""

    def test_get_info_returns_cadence_info(self, engine):
        """get_cadence_info should return CadenceInfo object."""
        # Arrange & Act
        info = engine.get_cadence_info(CadenceType.PERFECT_AUTHENTIC)
        # Assert
        assert isinstance(info, CadenceInfo)
        assert info.type == CadenceType.PERFECT_AUTHENTIC

    def test_get_info_has_numerals(self, engine):
        """Cadence info should have numeral progression."""
        # Arrange & Act
        info = engine.get_cadence_info(CadenceType.PERFECT_AUTHENTIC)
        # Assert
        assert info.numerals == ("V", "I")

    def test_get_info_has_description(self, engine):
        """Cadence info should have description."""
        # Arrange & Act
        info = engine.get_cadence_info(CadenceType.PLAGAL)
        # Assert
        assert "Amen" in info.description or "plagal" in info.description.lower()


# =============================================================================
# Extend With Cadence Tests
# =============================================================================


class TestExtendWithCadence:
    """Tests for extend_with_cadence method."""

    def test_extend_adds_cadence_numerals(self, engine):
        """extend_with_cadence should add cadence numerals."""
        # Arrange
        progression = ["I", "IV", "ii"]
        # Act
        extended = engine.extend_with_cadence(progression, CadenceType.PERFECT_AUTHENTIC)
        # Assert
        assert "V" in extended
        assert "I" in extended
        assert extended[-2:] == ["V", "I"] or extended[-1] == "I"

    def test_extend_preserves_original(self, engine):
        """extend_with_cadence should not modify original list."""
        # Arrange
        original = ["I", "IV"]
        progression = list(original)
        # Act
        engine.extend_with_cadence(progression, CadenceType.PLAGAL)
        # Assert
        assert progression == original


# =============================================================================
# List Cadence Types Tests
# =============================================================================


class TestListCadenceTypes:
    """Tests for list_cadence_types method."""

    def test_list_returns_all_types(self, engine):
        """list_cadence_types should return all cadence types."""
        # Arrange & Act
        types = engine.list_cadence_types()
        # Assert
        assert len(types) == len(CadenceType)

    def test_list_item_structure(self, engine):
        """Each item should have type, numerals, description, emotional_quality."""
        # Arrange & Act
        types = engine.list_cadence_types()
        # Assert
        for item in types:
            assert "type" in item
            assert "numerals" in item
            assert "description" in item
            assert "emotional_quality" in item


# =============================================================================
# Cadence Invariants Tests
# =============================================================================


class TestCadenceInvariants:
    """Tests for mathematical invariants and properties."""

    def test_invariant_all_cadences_have_notes(self, engine):
        """Invariant: All generated cadences must have notes."""
        for cad_type in CadenceType:
            # Arrange & Act
            chords = engine.create_cadence(cad_type, "C", "major", 4)
            # Assert
            for chord in chords:
                assert "notes" in chord
                assert isinstance(chord["notes"], list)
                assert len(chord["notes"]) >= 3, f"{cad_type.value}: chord must have at least 3 notes"

    def test_invariant_all_cadences_have_numerals(self, engine):
        """Invariant: All generated cadences must have Roman numerals."""
        for cad_type in CadenceType:
            # Arrange & Act
            chords = engine.create_cadence(cad_type, "C", "major", 4)
            # Assert
            for chord in chords:
                assert "numeral" in chord
                assert isinstance(chord["numeral"], str)
                assert len(chord["numeral"]) > 0

    def test_invariant_notes_are_valid_midi(self, engine):
        """Invariant: All notes must be valid MIDI values."""
        for cad_type in CadenceType:
            # Arrange & Act
            chords = engine.create_cadence(cad_type, "C", "major", 4)
            # Assert
            for chord in chords:
                for note in chord["notes"]:
                    assert 0 <= note <= 127, f"MIDI note {note} out of range"

    def test_invariant_deterministic_output(self, engine):
        """Invariant: Same input must produce same output."""
        # Arrange
        cad_type = CadenceType.PERFECT_AUTHENTIC
        key = "C"
        mode = "major"
        octave = 4
        # Act: call multiple times
        results = [engine.create_cadence(cad_type, key, mode, octave) for _ in range(10)]
        # Assert: all results identical
        for result in results[1:]:
            assert result == results[0]

    def test_invariant_authentic_cadences_end_on_tonic(self, engine):
        """Invariant: Authentic cadences must end on tonic (I or i)."""
        tonic_cadences = [
            CadenceType.PERFECT_AUTHENTIC,
            CadenceType.IMPERFECT_AUTHENTIC,
            CadenceType.PICARDY,
            CadenceType.NEAPOLITAN,
            CadenceType.BACKDOOR,
        ]
        for cad_type in tonic_cadences:
            # Arrange & Act
            chords = engine.create_cadence(cad_type, "C", "major", 4)
            # Assert
            last_numeral = chords[-1]["numeral"].upper().replace("6", "").replace("7", "")
            assert last_numeral == "I", f"{cad_type.value} should end on I, got {chords[-1]['numeral']}"

    def test_invariant_half_cadence_ends_on_dominant(self, engine):
        """Invariant: Half cadence must end on dominant (V)."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.HALF, "C", "major", 4)
        # Assert
        assert chords[-1]["numeral"] == "V"


# =============================================================================
# String Input Tests
# =============================================================================


class TestStringInput:
    """Tests for string-based cadence type input."""

    def test_create_cadence_with_string_type(self, engine):
        """create_cadence should accept string cadence type."""
        # Arrange & Act
        chords = engine.create_cadence("perfect_authentic", "C", "major", 4)
        # Assert
        assert len(chords) == 2
        assert chords[0]["numeral"] == "V"

    def test_create_cadence_string_case_insensitive(self, engine):
        """String cadence type should be case-insensitive."""
        # Arrange & Act
        chords1 = engine.create_cadence("PERFECT_AUTHENTIC", "C", "major", 4)
        chords2 = engine.create_cadence("perfect_authentic", "C", "major", 4)
        # Assert
        assert chords1 == chords2


# =============================================================================
# Edge Cases Tests
# =============================================================================


class TestCadenceEdgeCases:
    """Tests for edge cases and boundary conditions."""

    def test_octave_2_voicing(self, engine):
        """Cadences should work in low octave."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major", 2)
        # Assert: notes should be around MIDI 36-47
        for chord in chords:
            for note in chord["notes"]:
                assert note >= 24, f"Note {note} too low for octave 2"

    def test_octave_6_voicing(self, engine):
        """Cadences should work in high octave."""
        # Arrange & Act
        chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major", 6)
        # Assert: notes should be around MIDI 84-95
        for chord in chords:
            for note in chord["notes"]:
                assert note <= 127, f"Note {note} exceeds MIDI range"

    def test_enharmonic_keys(self, engine):
        """Cadences should work with enharmonic key spellings."""
        # Arrange & Act
        db_chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "Db", "major", 4)
        cs_chords = engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C#", "major", 4)
        # Assert: same pitch classes despite different spellings
        db_notes = set(n % 12 for chord in db_chords for n in chord["notes"])
        cs_notes = set(n % 12 for chord in cs_chords for n in chord["notes"])
        assert db_notes == cs_notes
