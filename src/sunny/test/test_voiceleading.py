"""Tests for the Voice Leading module."""

import pytest
from sunny.theory.voiceleading import (
    VoiceLeadingEngine,
    VoiceLeadingViolation,
)


class TestNearestToneVoicing:
    """Tests for the nearest-tone voice leading algorithm."""
    
    def test_minimal_movement(self):
        """Voice leading should minimize total pitch movement."""
        vl = VoiceLeadingEngine()
        
        # C major to D minor - should move stepwise
        c_major = [60, 64, 67]  # C-E-G
        d_minor = [62, 65, 69]  # D-F-A (as pitch classes)
        
        result = vl.nearest_tone_voicing(c_major, d_minor)
        
        # Check that movement is reasonable
        total_movement = sum(abs(r - o) for r, o in zip(result, c_major))
        assert total_movement <= 12  # Should be small
    
    def test_preserves_voice_count(self):
        """Should output same number of voices as input."""
        vl = VoiceLeadingEngine()
        
        current = [60, 64, 67, 72]
        target = [62, 65, 69]
        
        result = vl.nearest_tone_voicing(current, target)
        
        assert len(result) == len(current)
    
    def test_no_voice_crossings(self):
        """Result should not have voice crossings."""
        vl = VoiceLeadingEngine()
        
        current = [48, 60, 64, 67]  # Bass to soprano
        target = [50, 53, 57]
        
        result = vl.nearest_tone_voicing(current, target)
        
        # Each voice should be higher than the previous
        for i in range(len(result) - 1):
            assert result[i] <= result[i + 1]
    
    def test_lock_bass_keeps_root(self):
        """With lock_bass=True, bass should take chord root."""
        vl = VoiceLeadingEngine()
        
        current = [48, 60, 64, 67]
        target = [50, 53, 57]  # First is root
        
        result = vl.nearest_tone_voicing(current, target, lock_bass=True)
        
        # Bass should have same pitch class as first target
        assert result[0] % 12 == target[0] % 12


class TestParallelMotionDetection:
    """Tests for parallel 5ths and octaves detection."""
    
    def test_detect_parallel_fifths(self):
        """Should detect parallel perfect fifths."""
        vl = VoiceLeadingEngine()
        
        # C-G moving to D-A (parallel 5ths)
        voicing1 = [60, 67]
        voicing2 = [62, 69]
        
        violations = vl.check_parallel_fifths(voicing1, voicing2)
        
        assert len(violations) > 0
        assert violations[0].rule == "parallel_fifths"
    
    def test_no_false_positive_fifths(self):
        """Should not flag contrary or oblique motion."""
        vl = VoiceLeadingEngine()
        
        # C-G moving to D-F (contrary motion, not parallel)
        voicing1 = [60, 67]
        voicing2 = [62, 65]
        
        violations = vl.check_parallel_fifths(voicing1, voicing2)
        
        assert len(violations) == 0
    
    def test_detect_parallel_octaves(self):
        """Should detect parallel octaves/unisons."""
        vl = VoiceLeadingEngine()
        
        # C-C moving to D-D (parallel octaves)
        voicing1 = [60, 72]
        voicing2 = [62, 74]
        
        violations = vl.check_parallel_octaves(voicing1, voicing2)
        
        assert len(violations) > 0
        assert violations[0].rule == "parallel_octaves"


class TestVoiceCrossing:
    """Tests for voice crossing detection."""
    
    def test_detect_voice_crossing(self):
        """Should detect when lower voice is above upper voice."""
        vl = VoiceLeadingEngine()
        
        # Tenor crosses above alto
        voicing = [48, 67, 60, 72]  # Bass, Alto (wrong), Tenor, Soprano
        
        violations = vl.check_voice_crossing(voicing)
        
        assert len(violations) > 0
        assert violations[0].rule == "voice_crossing"
    
    def test_no_crossing_in_ordered_voicing(self):
        """Properly ordered voicing should have no crossings."""
        vl = VoiceLeadingEngine()
        
        voicing = [48, 55, 60, 67]  # Ascending
        
        violations = vl.check_voice_crossing(voicing)
        
        assert len(violations) == 0


class TestSpacing:
    """Tests for voice spacing checks."""
    
    def test_detect_excessive_spacing(self):
        """Should detect excessive spacing between upper voices."""
        vl = VoiceLeadingEngine()
        
        # Alto and tenor separated by more than octave
        voicing = [36, 48, 72, 76]  # Big gap between 48 and 72
        
        violations = vl.check_spacing(voicing, max_interval=12)
        
        assert len(violations) > 0
        assert violations[0].rule == "excessive_spacing"
    
    def test_accept_bass_separation(self):
        """Bass can be separated from tenor by more than octave."""
        vl = VoiceLeadingEngine()
        
        # Bass is far but that's OK, check starts from index 1
        voicing = [36, 60, 64, 67]
        
        violations = vl.check_spacing(voicing, max_interval=12)
        
        # Should not flag bass-tenor gap
        assert len(violations) == 0


class TestSmoothProgression:
    """Tests for smooth progression generation."""
    
    def test_generates_correct_number_of_voicings(self):
        """Should generate one voicing per chord."""
        vl = VoiceLeadingEngine()
        
        chords = [[0, 4, 7], [2, 5, 9], [0, 4, 7]]  # I-ii-I pitch classes
        
        result = vl.generate_smooth_progression(chords)
        
        assert len(result) == 3
    
    def test_smooth_transitions(self):
        """Adjacent voicings should have smooth motion."""
        vl = VoiceLeadingEngine()
        
        chords = [[60, 64, 67], [62, 65, 69], [60, 64, 67]]
        
        result = vl.generate_smooth_progression(chords)
        
        # Check that transitions are reasonably smooth
        for i in range(len(result) - 1):
            for j in range(min(len(result[i]), len(result[i + 1]))):
                movement = abs(result[i + 1][j] - result[i][j])
                assert movement <= 12  # No more than an octave leap


class TestTendencyTones:
    """Tests for tendency tone resolution."""
    
    def test_leading_tone_resolves_up(self):
        """Leading tone should resolve up by half step."""
        vl = VoiceLeadingEngine()
        
        leading_tone = 71  # B (leading tone in C major)
        key_root = 0  # C
        
        resolution = vl.resolve_leading_tone(leading_tone, key_root)
        
        assert resolution == 72  # C
    
    def test_seventh_resolves_down(self):
        """Chord seventh should resolve down."""
        vl = VoiceLeadingEngine()
        
        seventh = 65  # F (7th of G7)
        chord_root = 7  # G pitch class
        
        resolution = vl.resolve_seventh(seventh, chord_root)
        
        assert resolution < seventh  # Should go down


class TestInversions:
    """Tests for chord inversion operations."""
    
    def test_root_position(self):
        """Inversion 0 should return root position."""
        vl = VoiceLeadingEngine()
        
        chord = [60, 64, 67]
        
        result = vl.get_inversion(chord, 0)
        
        assert result == chord
    
    def test_first_inversion(self):
        """First inversion should put 3rd in bass."""
        vl = VoiceLeadingEngine()
        
        chord = [60, 64, 67]  # C-E-G
        
        result = vl.get_inversion(chord, 1)
        
        # Should be E-G-C (lowest should be E or E+octave)
        # After sorting: [64, 67, 72] or similar
        assert result[0] % 12 == 4  # E
    
    def test_second_inversion(self):
        """Second inversion should put 5th in bass."""
        vl = VoiceLeadingEngine()
        
        chord = [60, 64, 67]  # C-E-G
        
        result = vl.get_inversion(chord, 2)
        
        # Lowest should be G
        assert result[0] % 12 == 7  # G
    
    def test_optimal_inversion_for_bass(self):
        """Should find inversion with target bass."""
        vl = VoiceLeadingEngine()
        
        chord = [60, 64, 67]  # C-E-G
        
        result = vl.optimal_inversion_for_bass(chord, 4)  # Want E in bass
        
        assert result[0] % 12 == 4


class TestAllRulesCheck:
    """Tests for combined rule checking."""
    
    def test_clean_voicing_passes(self):
        """Well-crafted voice leading should pass all checks."""
        vl = VoiceLeadingEngine()
        
        # Careful voice leading: no parallel 5ths or octaves
        # C chord to F chord with good voice leading
        voicing1 = [48, 55, 60, 64]  # C-G-C-E (no P5 between adjacent voices)
        voicing2 = [48, 53, 60, 65]  # F-A-C-F (contrary motion, oblique bass)
        
        violations = vl.check_all_rules(voicing1, voicing2)
        
        # Filter for parallel motion specifically
        parallel_violations = [v for v in violations if "parallel" in v.rule]
        assert len(parallel_violations) == 0
    
    def test_bad_voicing_catches_violations(self):
        """Voice leading with issues should be flagged."""
        vl = VoiceLeadingEngine()
        
        voicing1 = [60, 67]  # C-G
        voicing2 = [62, 69]  # D-A (parallel 5ths)
        
        violations = vl.check_all_rules(voicing1, voicing2)
        
        assert len(violations) > 0
