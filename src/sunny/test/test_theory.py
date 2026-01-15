"""Tests for the Theory Engine.

These tests verify:
- Scale note generation for all modes
- Roman numeral chord progression generation
- Note quantization to scales
"""

from __future__ import annotations

import pytest

# Import with fallback for when music21 is not installed
try:
    from sunny.theory.engine import TheoryEngine
    ENGINE_AVAILABLE = True
except ImportError:
    ENGINE_AVAILABLE = False


@pytest.fixture
def engine():
    """Create a theory engine instance."""
    if not ENGINE_AVAILABLE:
        pytest.skip("Theory engine not available")
    return TheoryEngine()


class TestScaleNotes:
    """Tests for scale note generation."""
    
    def test_c_major_scale(self, engine):
        """C major (Ionian) should produce correct MIDI notes."""
        notes = engine.get_scale_notes("C", "ionian", 4)
        
        # C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71
        expected = [60, 62, 64, 65, 67, 69, 71]
        assert notes == expected
    
    def test_c_major_alias(self, engine):
        """Major should be an alias for Ionian."""
        ionian = engine.get_scale_notes("C", "ionian", 4)
        major = engine.get_scale_notes("C", "major", 4)
        assert ionian == major
    
    def test_dorian_mode(self, engine):
        """Dorian mode should have correct intervals."""
        notes = engine.get_scale_notes("D", "dorian", 4)
        
        # D Dorian: D E F G A B C
        # D4=62, intervals: 0,2,3,5,7,9,10
        expected = [62, 64, 65, 67, 69, 71, 72]
        assert notes == expected
    
    def test_phrygian_mode(self, engine):
        """Phrygian mode should have b2 interval."""
        notes = engine.get_scale_notes("E", "phrygian", 4)
        
        # E Phrygian characteristic: half step from root to 2nd
        # E4=64, F4=65 (half step)
        assert notes[1] - notes[0] == 1  # b2 interval
    
    def test_lydian_mode(self, engine):
        """Lydian mode should have #4 interval."""
        notes = engine.get_scale_notes("F", "lydian", 4)
        
        # F Lydian: F G A B C D E
        # F4=65, B4=71 is #4 (tritone from root)
        root = notes[0]
        fourth = notes[3]
        assert fourth - root == 6  # Tritone (#4)
    
    def test_mixolydian_mode(self, engine):
        """Mixolydian mode should have b7 interval."""
        notes = engine.get_scale_notes("G", "mixolydian", 4)
        
        # G Mixolydian: G A B C D E F
        # G4=67, F5=77 but we're in octave 4
        root = notes[0]
        seventh = notes[6]
        assert seventh - root == 10  # Minor 7th (b7)
    
    def test_aeolian_mode(self, engine):
        """Aeolian (natural minor) intervals."""
        notes = engine.get_scale_notes("A", "aeolian", 4)
        
        # A Aeolian: A B C D E F G
        # A4=69
        expected = [69, 71, 72, 74, 76, 77, 79]
        assert notes == expected
    
    def test_locrian_mode(self, engine):
        """Locrian mode should have b2 and b5."""
        notes = engine.get_scale_notes("B", "locrian", 4)
        
        # B Locrian: B C D E F G A
        root = notes[0]
        second = notes[1]
        fifth = notes[4]
        
        assert second - root == 1  # b2
        assert fifth - root == 6   # b5 (tritone)
    
    def test_invalid_mode_raises(self, engine):
        """Invalid mode name should raise ValueError."""
        with pytest.raises(ValueError, match="Unknown mode"):
            engine.get_scale_notes("C", "invalid_mode", 4)
    
    def test_different_octaves(self, engine):
        """Scale notes should transpose correctly across octaves."""
        octave3 = engine.get_scale_notes("C", "major", 3)
        octave4 = engine.get_scale_notes("C", "major", 4)
        octave5 = engine.get_scale_notes("C", "major", 5)
        
        # Should be 12 semitones apart
        assert octave4[0] - octave3[0] == 12
        assert octave5[0] - octave4[0] == 12
    
    def test_sharp_root(self, engine):
        """Sharp roots should be parsed correctly."""
        notes = engine.get_scale_notes("F#", "major", 4)
        
        # F#4 = 66
        assert notes[0] == 66
    
    def test_flat_root(self, engine):
        """Flat roots should be parsed correctly."""
        notes = engine.get_scale_notes("Bb", "major", 4)
        
        # Bb4 = 70
        assert notes[0] == 70


class TestChordProgression:
    """Tests for chord progression generation."""
    
    def test_ii_V_I_major(self, engine):
        """ii-V-I in C major should produce Dm, G, C."""
        prog = engine.generate_progression("C", "major", ["ii", "V", "I"], 4)
        
        assert len(prog) == 3
        
        # ii = Dm
        assert prog[0]["numeral"] == "ii"
        assert prog[0]["quality"] in ("minor", "min")
        
        # V = G
        assert prog[1]["numeral"] == "V"
        assert prog[1]["quality"] in ("major", "maj")
        
        # I = C
        assert prog[2]["numeral"] == "I"
        assert prog[2]["quality"] in ("major", "maj")
    
    def test_ii_V_I_notes(self, engine):
        """ii-V-I should contain correct MIDI notes."""
        prog = engine.generate_progression("C", "major", ["ii", "V", "I"], 4)
        
        # Dm chord should contain D, F, A
        dm_notes = prog[0]["notes"]
        assert 62 in dm_notes or 50 in dm_notes  # D
        assert 65 in dm_notes or 53 in dm_notes  # F
        assert 69 in dm_notes or 57 in dm_notes  # A
    
    def test_i_iv_v_minor(self, engine):
        """i-iv-V in A minor."""
        prog = engine.generate_progression("A", "minor", ["i", "iv", "V"], 4)
        
        assert len(prog) == 3
        
        # i = Am
        assert prog[0]["quality"] in ("minor", "min")
        
        # iv = Dm
        assert prog[1]["quality"] in ("minor", "min")
    
    def test_empty_numerals(self, engine):
        """Empty numeral list should return empty progression."""
        prog = engine.generate_progression("C", "major", [], 4)
        assert prog == []
    
    def test_single_numeral(self, engine):
        """Single numeral should work."""
        prog = engine.generate_progression("C", "major", ["I"], 4)
        assert len(prog) == 1
        assert prog[0]["numeral"] == "I"


class TestQuantization:
    """Tests for scale quantization."""
    
    def test_in_scale_notes_unchanged(self, engine):
        """Notes already in scale should not change."""
        c_major = [60, 64, 67]  # C, E, G (all in C major)
        quantized = engine.quantize_to_scale(c_major, "C", "major")
        assert quantized == c_major
    
    def test_out_of_scale_snapped(self, engine):
        """Notes outside scale should snap to nearest."""
        # C# should snap to C or D
        notes = [61]  # C#
        quantized = engine.quantize_to_scale(notes, "C", "major")
        assert quantized[0] in (60, 62)  # C or D
    
    def test_accidental_snapping(self, engine):
        """All accidentals should snap to scale tones."""
        # All notes in octave 4
        chromatic = list(range(60, 72))
        quantized = engine.quantize_to_scale(chromatic, "C", "major")
        
        # All should be white keys
        c_major_pitches = {60, 62, 64, 65, 67, 69, 71}
        for note in quantized:
            assert note % 12 + 60 in c_major_pitches or note % 12 in {0, 2, 4, 5, 7, 9, 11}


class TestNoteNameParsing:
    """Tests for note name parsing."""
    
    def test_natural_notes(self, engine):
        """Natural note names should parse correctly."""
        assert engine._parse_note_name("C") == 0
        assert engine._parse_note_name("D") == 2
        assert engine._parse_note_name("E") == 4
        assert engine._parse_note_name("F") == 5
        assert engine._parse_note_name("G") == 7
        assert engine._parse_note_name("A") == 9
        assert engine._parse_note_name("B") == 11
    
    def test_sharp_notes(self, engine):
        """Sharp notes should parse correctly."""
        assert engine._parse_note_name("C#") == 1
        assert engine._parse_note_name("F#") == 6
    
    def test_flat_notes(self, engine):
        """Flat notes should parse correctly."""
        assert engine._parse_note_name("Db") == 1
        assert engine._parse_note_name("Bb") == 10
    
    def test_empty_raises(self, engine):
        """Empty note name should raise ValueError."""
        with pytest.raises(ValueError):
            engine._parse_note_name("")
    
    def test_invalid_raises(self, engine):
        """Invalid note name should raise ValueError."""
        with pytest.raises(ValueError):
            engine._parse_note_name("X")
