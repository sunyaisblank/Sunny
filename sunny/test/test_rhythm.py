"""Tests for the Rhythm Engine.

These tests verify:
- Polyrhythm generation
- Tuplet creation
- Swing application
- Euclidean rhythm patterns
"""

from __future__ import annotations

import pytest

try:
    from Sunny.Theory.rhythm import RhythmEngine, NoteEvent
    ENGINE_AVAILABLE = True
except ImportError:
    ENGINE_AVAILABLE = False


@pytest.fixture
def engine():
    """Create a rhythm engine instance."""
    if not ENGINE_AVAILABLE:
        pytest.skip("Rhythm engine not available")
    return RhythmEngine()


class TestPolyrhythm:
    """Tests for polyrhythm generation."""
    
    def test_3_against_2(self, engine):
        """3:2 polyrhythm should have correct positions."""
        result = engine.create_polyrhythm((3, 1), (2, 1), 4.0)
        
        assert len(result["a"]) == 3
        assert len(result["b"]) == 2
        
        # Pattern A divides 4 beats into 3
        assert result["a"][0] == pytest.approx(0.0)
        assert result["a"][1] == pytest.approx(4.0 / 3)
        assert result["a"][2] == pytest.approx(8.0 / 3)
        
        # Pattern B divides 4 beats into 2
        assert result["b"][0] == pytest.approx(0.0)
        assert result["b"][1] == pytest.approx(2.0)
    
    def test_5_against_4(self, engine):
        """5:4 polyrhythm should work correctly."""
        result = engine.create_polyrhythm((5, 1), (4, 1), 4.0)
        
        assert len(result["a"]) == 5
        assert len(result["b"]) == 4
    
    def test_combined_positions(self, engine):
        """Combined positions should include all unique positions."""
        result = engine.create_polyrhythm((3, 1), (2, 1), 4.0)
        
        combined = result["combined"]
        
        # Should have all positions from both patterns
        for pos in result["a"]:
            assert any(abs(p - pos) < 0.001 for p in combined)
        for pos in result["b"]:
            assert any(abs(p - pos) < 0.001 for p in combined)


class TestTuplet:
    """Tests for tuplet generation."""
    
    def test_triplet(self, engine):
        """Triplet should fit 3 in space of 2."""
        triplet = engine.create_tuplet(3, 2, 0.5)
        
        assert len(triplet) == 3
        
        # Total duration should equal 2 * 0.5 = 1 beat
        assert sum(triplet) == pytest.approx(1.0)
        
        # Each note should be 1/3 beat
        for duration in triplet:
            assert duration == pytest.approx(1.0 / 3)
    
    def test_quintuplet(self, engine):
        """Quintuplet should fit 5 in space of 4."""
        quint = engine.create_tuplet(5, 4, 0.25)
        
        assert len(quint) == 5
        
        # Total duration should equal 4 * 0.25 = 1 beat
        assert sum(quint) == pytest.approx(1.0)
    
    def test_tuplet_positions(self, engine):
        """Tuplet positions should start at specified beat."""
        positions = engine.create_tuplet_positions(3, 2, 0.5, start_beat=2.0)
        
        assert positions[0] == pytest.approx(2.0)
        assert len(positions) == 3


class TestSwing:
    """Tests for swing application."""
    
    def test_zero_swing_unchanged(self, engine):
        """Zero swing should not modify timings."""
        notes = [
            {"start_time": 0.0, "pitch": 60},
            {"start_time": 0.25, "pitch": 62},
            {"start_time": 0.5, "pitch": 64},
            {"start_time": 0.75, "pitch": 65},
        ]
        
        swung = engine.apply_swing(notes, 0.0, "16th")
        
        for orig, new in zip(notes, swung):
            assert new["start_time"] == pytest.approx(orig["start_time"])
    
    def test_swing_delays_offbeats(self, engine):
        """Swing should delay offbeat notes."""
        notes = [
            {"start_time": 0.0, "pitch": 60},   # Downbeat
            {"start_time": 0.25, "pitch": 62},  # Offbeat
        ]
        
        swung = engine.apply_swing(notes, 0.5, "16th")
        
        # Downbeat unchanged
        assert swung[0]["start_time"] == pytest.approx(0.0)
        
        # Offbeat delayed
        assert swung[1]["start_time"] > 0.25
    
    def test_full_swing(self, engine):
        """Full swing should apply maximum delay."""
        notes = [{"start_time": 0.25, "pitch": 60}]
        
        swung = engine.apply_swing(notes, 1.0, "16th")
        
        # Should be noticeably delayed
        assert swung[0]["start_time"] > 0.3


class TestEuclidean:
    """Tests for Euclidean rhythm generation."""
    
    def test_tresillo(self, engine):
        """E(3,8) should produce tresillo pattern."""
        pattern = engine.create_euclidean_rhythm(3, 8)
        
        assert len(pattern) == 8
        assert sum(pattern) == 3  # 3 hits
        
        # Euclidean property: pulses should be maximally spread
        # The gaps between hits should be roughly equal (either 2 or 3)
        hit_positions = [i for i, hit in enumerate(pattern) if hit]
        gaps = []
        for i in range(len(hit_positions)):
            next_pos = hit_positions[(i + 1) % len(hit_positions)]
            if next_pos <= hit_positions[i]:
                next_pos += 8  # Wrap around
            gaps.append(next_pos - hit_positions[i])
        
        # All gaps should be 2 or 3 (maximally even distribution)
        for gap in gaps:
            assert gap in (2, 3), f"Gap {gap} not in [2, 3]"
    
    def test_cinquillo(self, engine):
        """E(5,8) should produce cinquillo pattern."""
        pattern = engine.create_euclidean_rhythm(5, 8)
        
        assert len(pattern) == 8
        assert sum(pattern) == 5
    
    def test_all_hits(self, engine):
        """E(n,n) should be all hits."""
        pattern = engine.create_euclidean_rhythm(8, 8)
        
        assert all(pattern)
    
    def test_no_hits(self, engine):
        """E(0,n) should be all rests."""
        pattern = engine.create_euclidean_rhythm(0, 8)
        
        assert not any(pattern)
    
    def test_pulses_exceed_steps(self, engine):
        """More pulses than steps should raise error."""
        with pytest.raises(ValueError):
            engine.create_euclidean_rhythm(10, 8)
    
    def test_rotation(self, engine):
        """Rotation should shift pattern."""
        original = engine.create_euclidean_rhythm(3, 8, rotation=0)
        rotated = engine.create_euclidean_rhythm(3, 8, rotation=1)
        
        # Rotation shifts pattern
        assert rotated[0] == original[1]
    
    def test_to_positions(self, engine):
        """Pattern should convert to note events."""
        pattern = engine.create_euclidean_rhythm(3, 8)
        events = engine.euclidean_to_positions(pattern, 0.5)
        
        assert len(events) == 3
        assert all(isinstance(e, NoteEvent) for e in events)


class TestQuantization:
    """Tests for rhythm quantization."""
    
    def test_on_grid_unchanged(self, engine):
        """Notes on grid should not move."""
        notes = [{"start_time": 0.0}, {"start_time": 0.25}]
        
        quantized = engine.quantize_to_grid(notes, "16th", 1.0)
        
        assert quantized[0]["start_time"] == pytest.approx(0.0)
        assert quantized[1]["start_time"] == pytest.approx(0.25)
    
    def test_off_grid_snapped(self, engine):
        """Notes off grid should snap."""
        notes = [{"start_time": 0.1}]  # Between 0 and 0.25
        
        quantized = engine.quantize_to_grid(notes, "16th", 1.0)
        
        # Should snap to 0 or 0.25
        assert quantized[0]["start_time"] in (0.0, 0.25)
    
    def test_zero_strength_unchanged(self, engine):
        """Zero strength should not modify notes."""
        notes = [{"start_time": 0.1}]
        
        quantized = engine.quantize_to_grid(notes, "16th", 0.0)
        
        assert quantized[0]["start_time"] == pytest.approx(0.1)
    
    def test_partial_strength(self, engine):
        """Partial strength should move note partially."""
        notes = [{"start_time": 0.1}]
        
        quantized = engine.quantize_to_grid(notes, "16th", 0.5)
        
        # Should be between original and grid
        assert 0.0 < quantized[0]["start_time"] < 0.1
