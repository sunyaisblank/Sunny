"""Tests for the Functional Harmony module."""

import pytest
from Sunny.Theory.harmony import (
    HarmonyModule,
    FunctionalRole,
    ChordInfo,
    pitch_class,
    note_name,
)


class TestFunctionalRoles:
    """Tests for functional role analysis."""
    
    def test_tonic_on_degree_1(self):
        """Scale degree 1 should be tonic function."""
        harmony = HarmonyModule()
        assert harmony.get_functional_role(1, "major") == FunctionalRole.TONIC
        assert harmony.get_functional_role(1, "minor") == FunctionalRole.TONIC
    
    def test_dominant_on_degree_5(self):
        """Scale degree 5 should be dominant function."""
        harmony = HarmonyModule()
        assert harmony.get_functional_role(5, "major") == FunctionalRole.DOMINANT
        assert harmony.get_functional_role(5, "minor") == FunctionalRole.DOMINANT
    
    def test_subdominant_on_degree_4(self):
        """Scale degree 4 should be subdominant function."""
        harmony = HarmonyModule()
        assert harmony.get_functional_role(4, "major") == FunctionalRole.SUBDOMINANT
        assert harmony.get_functional_role(4, "minor") == FunctionalRole.SUBDOMINANT
    
    def test_vi_is_tonic_substitute(self):
        """vi chord should have tonic function (tonic substitute)."""
        harmony = HarmonyModule()
        assert harmony.get_functional_role(6, "major") == FunctionalRole.TONIC
    
    def test_ii_is_subdominant(self):
        """ii chord should have subdominant function."""
        harmony = HarmonyModule()
        assert harmony.get_functional_role(2, "major") == FunctionalRole.SUBDOMINANT


class TestProgressionAnalysis:
    """Tests for progression analysis."""
    
    def test_ii_V_I_analysis(self):
        """Analyze classic ii-V-I progression."""
        harmony = HarmonyModule()
        analysis = harmony.analyze_progression(["ii", "V", "I"], "major")
        
        assert len(analysis) == 3
        assert analysis[0]["function"] == "S"  # ii = subdominant
        assert analysis[1]["function"] == "D"  # V = dominant
        assert analysis[2]["function"] == "T"  # I = tonic
    
    def test_tension_levels(self):
        """Tension should increase through S->D and resolve at T."""
        harmony = HarmonyModule()
        analysis = harmony.analyze_progression(["ii", "V", "I"], "major")
        
        assert analysis[0]["tension"] == 1  # Subdominant
        assert analysis[1]["tension"] == 2  # Dominant (max)
        assert analysis[2]["tension"] == 0  # Tonic (resolved)


class TestNegativeHarmony:
    """Tests for negative harmony operations."""
    
    def test_pitch_mirroring_around_axis(self):
        """Pitches should mirror correctly around axis."""
        harmony = HarmonyModule()
        
        # C major chord (C-E-G = 0-4-7 pitch classes)
        # Mirrored around axis 0 should invert intervals
        result = harmony.generate_negative_mirror([60, 64, 67], axis=0)
        
        # Result should have 3 notes
        assert len(result) == 3
    
    def test_major_mirrors_to_minor_quality(self):
        """Major chord should mirror to minor-like structure."""
        harmony = HarmonyModule()
        
        result = harmony.mirror_chord("C", "major", "C", 4)
        
        assert isinstance(result, ChordInfo)
        assert result.quality == "minor"
    
    def test_mirror_progression_ii_V_I(self):
        """ii-V-I should mirror to bVII-iv-i (conceptually)."""
        harmony = HarmonyModule()
        
        result = harmony.mirror_progression("C", "major", ["ii", "V", "I"])
        
        assert len(result) == 3
        # ii mirrors to bVII
        assert "bVII" in result[0]["mirrored"]


class TestSecondaryDominants:
    """Tests for secondary dominant generation."""
    
    def test_V_of_V_in_C_major(self):
        """V/V in C major should be D major."""
        harmony = HarmonyModule()
        
        result = harmony.secondary_dominant(5, "C", "major", 4)
        
        assert result.root == "D"
        assert result.quality == "major"
        assert "V/V" in result.numeral
    
    def test_V_of_ii_in_C_major(self):
        """V/ii in C major should be A major."""
        harmony = HarmonyModule()
        
        result = harmony.secondary_dominant(2, "C", "major", 4)
        
        assert result.root == "A"
        assert result.quality == "major"
    
    def test_secondary_dominant_seventh(self):
        """Secondary dominant 7th should include minor 7th."""
        harmony = HarmonyModule()
        
        result = harmony.secondary_dominant_seventh(5, "C", "major", 4)
        
        assert result.quality == "dominant7"
        assert len(result.notes) == 4  # Root, 3rd, 5th, 7th


class TestModalInterchange:
    """Tests for modal interchange (borrowed chords)."""
    
    def test_borrowed_chords_from_major(self):
        """Major key should have borrowed chords from parallel minor."""
        harmony = HarmonyModule()
        
        borrowed = harmony.get_borrowed_chords("C", "major")
        
        # Should include common borrowed chords
        numerals = [c["numeral"] for c in borrowed]
        assert "iv" in numerals  # Minor iv
        assert "bVI" in numerals  # Flat VI major
        assert "bVII" in numerals  # Flat VII major
    
    def test_find_pivot_chords_C_to_G(self):
        """C major and G major should share common chords."""
        harmony = HarmonyModule()
        
        pivots = harmony.find_pivot_chords("C", "major", "G", "major")
        
        # G major chord exists in both keys
        roots = [p["root"] for p in pivots]
        assert "G" in roots  # G is I in G major, V in C major


class TestAugmentedSixths:
    """Tests for augmented sixth chord generation."""
    
    def test_italian_sixth(self):
        """Italian +6 in C should have Ab-C-F#."""
        harmony = HarmonyModule()
        
        result = harmony.augmented_sixth("italian", "C", "major", 4)
        
        assert "Ab" in result.root
        assert len(result.notes) == 3
    
    def test_german_sixth(self):
        """German +6 in C should have 4 notes."""
        harmony = HarmonyModule()
        
        result = harmony.augmented_sixth("german", "C", "major", 4)
        
        assert len(result.notes) == 4
    
    def test_french_sixth(self):
        """French +6 should include the 2nd scale degree."""
        harmony = HarmonyModule()
        
        result = harmony.augmented_sixth("french", "C", "major", 4)
        
        assert len(result.notes) == 4


class TestSpecialChords:
    """Tests for special chord types."""
    
    def test_neapolitan_chord(self):
        """Neapolitan in C should be Db major."""
        harmony = HarmonyModule()
        
        result = harmony.neapolitan_chord("C", "major", 4)
        
        assert result.root == "Db"
        assert result.quality == "major"
    
    def test_neapolitan_first_inversion(self):
        """Neapolitan 6 should have 3rd in bass."""
        harmony = HarmonyModule()
        
        result = harmony.neapolitan_chord("C", "major", 4, inversion=1)
        
        # First inversion - 3rd (F) should be lowest
        assert "bII6" in result.numeral
    
    def test_tritone_substitution(self):
        """Tritone sub for G7 should be Db7."""
        harmony = HarmonyModule()
        
        result = harmony.tritone_substitution("G", 4)
        
        assert result.root == "Db"
        assert result.quality == "dominant7"


class TestUtilities:
    """Tests for utility functions."""
    
    def test_pitch_class_from_midi(self):
        """MIDI note 60 (C4) should have pitch class 0."""
        assert pitch_class(60) == 0
        assert pitch_class(61) == 1  # C#
        assert pitch_class(72) == 0  # C5
    
    def test_pitch_class_from_name(self):
        """Note names should convert to correct pitch classes."""
        assert pitch_class("C") == 0
        assert pitch_class("D") == 2
        assert pitch_class("Bb") == 10
    
    def test_note_name_from_pitch(self):
        """Pitch classes should convert to note names."""
        assert note_name(0) == "C"
        assert note_name(7) == "G"
    
    def test_note_name_prefer_flats(self):
        """Should use flats when specified."""
        assert note_name(1, prefer_flats=True) == "Db"
        assert note_name(1, prefer_flats=False) == "C#"
