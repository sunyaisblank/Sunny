"""Tests for the Orchestration Guide.

Tests for instrument suggestions and timbral guidance.
"""

from __future__ import annotations

import pytest


try:
    from Sunny.Theory.orchestration import (
        OrchestrationGuide,
        InstrumentRole,
        EmotionalColor,
    )
    ORCHESTRATION_AVAILABLE = True
except ImportError:
    ORCHESTRATION_AVAILABLE = False


@pytest.fixture
def guide():
    """Create an orchestration guide instance."""
    if not ORCHESTRATION_AVAILABLE:
        pytest.skip("Orchestration not available")
    return OrchestrationGuide()


class TestEmotionalColorSuggestions:
    """Tests for emotion-based instrument suggestions."""
    
    def test_heroic_suggests_brass(self, guide):
        """Heroic emotion should suggest brass instruments."""
        suggestions = guide.suggest_instruments("heroic")
        
        assert len(suggestions) > 0
        names = [s["name"].lower() for s in suggestions]
        assert any("horn" in n or "trumpet" in n or "trombone" in n for n in names)
    
    def test_melancholy_suggests_strings(self, guide):
        """Melancholy emotion should suggest strings."""
        suggestions = guide.suggest_instruments("melancholy")
        
        assert len(suggestions) > 0
        names = [s["name"].lower() for s in suggestions]
        assert any("cello" in n or "viola" in n or "oboe" in n for n in names)
    
    def test_ethereal_suggests_high_instruments(self, guide):
        """Ethereal emotion should suggest high, delicate instruments."""
        suggestions = guide.suggest_instruments("ethereal")
        
        assert len(suggestions) > 0
        names = [s["name"].lower() for s in suggestions]
        assert any("celesta" in n or "harp" in n or "flute" in n for n in names)
    
    def test_climactic_suggests_full_orchestra(self, guide):
        """Climactic emotion should suggest powerful instruments."""
        suggestions = guide.suggest_instruments("climactic")
        
        assert len(suggestions) > 0
        categories = [s["category"] for s in suggestions]
        # Should include multiple categories
        assert len(set(categories)) >= 2
    
    def test_unknown_emotion_defaults(self, guide):
        """Unknown emotion should return results without error."""
        suggestions = guide.suggest_instruments("nonexistent_emotion")
        # Should default gracefully
        assert isinstance(suggestions, list)


class TestRegisterFiltering:
    """Tests for register-based filtering."""
    
    def test_filter_by_bass_register(self, guide):
        """Should filter to only bass instruments."""
        suggestions = guide.suggest_instruments("heroic", register="bass")
        
        for s in suggestions:
            assert s["register"] == "bass"
    
    def test_filter_by_soprano_register(self, guide):
        """Should filter to only soprano instruments."""
        suggestions = guide.suggest_instruments("pastoral", register="soprano")
        
        for s in suggestions:
            assert s["register"] == "soprano"


class TestCategoryFiltering:
    """Tests for category-based filtering."""
    
    def test_filter_by_brass_category(self, guide):
        """Should filter to only brass instruments."""
        suggestions = guide.suggest_instruments("heroic", category="brass")
        
        for s in suggestions:
            assert s["category"] == "brass"
    
    def test_filter_by_strings_category(self, guide):
        """Should filter to only string instruments."""
        suggestions = guide.suggest_instruments("romantic", category="strings")
        
        for s in suggestions:
            assert s["category"] == "strings"


class TestVoiceAssignment:
    """Tests for chord voice assignment."""
    
    def test_assigns_bass_to_lowest_note(self, guide):
        """Lowest note should be assigned to bass."""
        chord = [36, 48, 55, 60]  # C2, C3, G3, C4
        
        assignment = guide.get_voice_assignment(chord)
        
        assert len(assignment["bass"]) > 0
        bass_notes = [n["note"] for n in assignment["bass"]]
        assert 36 in bass_notes
    
    def test_assigns_all_notes(self, guide):
        """All notes should be assigned to some role."""
        chord = [36, 48, 55, 60, 67]
        
        assignment = guide.get_voice_assignment(chord)
        
        all_assigned = []
        for role in assignment.values():
            all_assigned.extend([n["note"] for n in role])
        
        assert len(all_assigned) == len(chord)
    
    def test_suggests_instruments_per_note(self, guide):
        """Each note should have instrument suggestions."""
        chord = [48, 60, 67]
        
        assignment = guide.get_voice_assignment(chord)
        
        for role, notes in assignment.items():
            for note in notes:
                assert "suggested_instruments" in note
                assert len(note["suggested_instruments"]) > 0


class TestAbletonMapping:
    """Tests for Ableton device URI mapping."""
    
    def test_maps_known_instrument(self, guide):
        """Should return URIs for known instruments."""
        uris = guide.get_ableton_device_suggestions("violin")
        
        assert len(uris) > 0
        assert "Violin" in uris[0] or "violin" in uris[0].lower()
    
    def test_returns_empty_for_unknown(self, guide):
        """Should return empty list for unknown instruments."""
        uris = guide.get_ableton_device_suggestions("nonexistent_instrument")
        
        assert uris == []


class TestInstrumentListing:
    """Tests for instrument listing."""
    
    def test_list_all_instruments(self, guide):
        """Should list all available instruments."""
        instruments = guide.list_instruments()
        
        assert len(instruments) > 20  # Should have many instruments
    
    def test_list_by_category(self, guide):
        """Should filter by category."""
        brass = guide.list_instruments(category="brass")
        
        assert len(brass) > 0
        for inst in brass:
            assert inst["category"] == "brass"


class TestEmotionalColorListing:
    """Tests for emotional color discovery."""
    
    def test_lists_all_colors(self, guide):
        """Should list all emotional colors."""
        colors = guide.list_emotional_colors()
        
        assert len(colors) == len(EmotionalColor)
    
    def test_includes_sample_instruments(self, guide):
        """Each color should include sample instruments."""
        colors = guide.list_emotional_colors()
        
        for color in colors:
            assert "sample_instruments" in color
            assert "color" in color
