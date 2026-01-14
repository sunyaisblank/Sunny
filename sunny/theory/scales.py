"""Sunny - Scale System.

Component: THSC001A
Domain: TH (Theory) | Category: SC (Scale)

Comprehensive scale and mode definitions with interval patterns
and chord quality mappings. Supports 40+ scales including church
modes, exotic scales, symmetric scales, and bebop scales.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import NamedTuple


class Interval(Enum):
    """Musical intervals in semitones."""
    
    UNISON = 0
    MINOR_SECOND = 1
    MAJOR_SECOND = 2
    MINOR_THIRD = 3
    MAJOR_THIRD = 4
    PERFECT_FOURTH = 5
    TRITONE = 6
    PERFECT_FIFTH = 7
    MINOR_SIXTH = 8
    MAJOR_SIXTH = 9
    MINOR_SEVENTH = 10
    MAJOR_SEVENTH = 11
    OCTAVE = 12


class ScaleInfo(NamedTuple):
    """Scale information with intervals and characteristics."""
    
    name: str
    intervals: tuple[int, ...]
    chord_qualities: tuple[str, ...]  # Quality for each scale degree
    description: str


# Church Modes (diatonic modes)
CHURCH_MODES = {
    "ionian": ScaleInfo(
        name="Ionian (Major)",
        intervals=(0, 2, 4, 5, 7, 9, 11),
        chord_qualities=("maj", "min", "min", "maj", "maj", "min", "dim"),
        description="The major scale - bright, happy, resolved"
    ),
    "dorian": ScaleInfo(
        name="Dorian",
        intervals=(0, 2, 3, 5, 7, 9, 10),
        chord_qualities=("min", "min", "maj", "maj", "min", "dim", "maj"),
        description="Minor with raised 6th - jazzy, sophisticated"
    ),
    "phrygian": ScaleInfo(
        name="Phrygian",
        intervals=(0, 1, 3, 5, 7, 8, 10),
        chord_qualities=("min", "maj", "maj", "min", "dim", "maj", "min"),
        description="Minor with b2 - Spanish, exotic, dark"
    ),
    "lydian": ScaleInfo(
        name="Lydian",
        intervals=(0, 2, 4, 6, 7, 9, 11),
        chord_qualities=("maj", "maj", "min", "dim", "maj", "min", "min"),
        description="Major with #4 - dreamy, floating, ethereal"
    ),
    "mixolydian": ScaleInfo(
        name="Mixolydian",
        intervals=(0, 2, 4, 5, 7, 9, 10),
        chord_qualities=("maj", "min", "dim", "maj", "min", "min", "maj"),
        description="Major with b7 - bluesy, rock, dominant"
    ),
    "aeolian": ScaleInfo(
        name="Aeolian (Natural Minor)",
        intervals=(0, 2, 3, 5, 7, 8, 10),
        chord_qualities=("min", "dim", "maj", "min", "min", "maj", "maj"),
        description="Natural minor - sad, dark, emotional"
    ),
    "locrian": ScaleInfo(
        name="Locrian",
        intervals=(0, 1, 3, 5, 6, 8, 10),
        chord_qualities=("dim", "maj", "min", "min", "maj", "maj", "min"),
        description="Diminished tonic - unstable, tense, rare"
    ),
}

# Additional Minor Scales
MINOR_SCALES = {
    "harmonic_minor": ScaleInfo(
        name="Harmonic Minor",
        intervals=(0, 2, 3, 5, 7, 8, 11),
        chord_qualities=("min", "dim", "aug", "min", "maj", "maj", "dim"),
        description="Natural minor with raised 7th - classical, dramatic"
    ),
    "melodic_minor": ScaleInfo(
        name="Melodic Minor",
        intervals=(0, 2, 3, 5, 7, 9, 11),
        chord_qualities=("min", "min", "aug", "maj", "maj", "dim", "dim"),
        description="Minor with raised 6 and 7 - jazz, ascending"
    ),
}

# Pentatonic Scales
PENTATONIC_SCALES = {
    "major_pentatonic": ScaleInfo(
        name="Major Pentatonic",
        intervals=(0, 2, 4, 7, 9),
        chord_qualities=("maj", "min", "min", "maj", "min"),
        description="5-note major - universal, folk, safe"
    ),
    "minor_pentatonic": ScaleInfo(
        name="Minor Pentatonic",
        intervals=(0, 3, 5, 7, 10),
        chord_qualities=("min", "maj", "min", "min", "maj"),
        description="5-note minor - blues, rock, soul"
    ),
}

# Blues Scales
BLUES_SCALES = {
    "blues": ScaleInfo(
        name="Blues",
        intervals=(0, 3, 5, 6, 7, 10),
        chord_qualities=("min", "maj", "min", "dim", "min", "maj"),
        description="Minor pentatonic + blue note"
    ),
    "major_blues": ScaleInfo(
        name="Major Blues",
        intervals=(0, 2, 3, 4, 7, 9),
        chord_qualities=("maj", "min", "min", "maj", "maj", "min"),
        description="Major pentatonic + blue note"
    ),
}

# Symmetric Scales (Messiaen Modes / Deep Iceberg)
SYMMETRIC_SCALES = {
    "octatonic_hw": ScaleInfo(
        name="Octatonic (Half-Whole Diminished)",
        intervals=(0, 1, 3, 4, 6, 7, 9, 10),
        chord_qualities=("dim", "min", "dim", "min", "dim", "min", "dim", "min"),
        description="8-note symmetric - jazz, film scoring, tension"
    ),
    "octatonic_wh": ScaleInfo(
        name="Octatonic (Whole-Half Diminished)",
        intervals=(0, 2, 3, 5, 6, 8, 9, 11),
        chord_qualities=("min", "dim", "min", "dim", "min", "dim", "min", "dim"),
        description="8-note symmetric - mystery, ambiguity"
    ),
    "whole_tone": ScaleInfo(
        name="Whole Tone",
        intervals=(0, 2, 4, 6, 8, 10),
        chord_qualities=("aug", "aug", "aug", "aug", "aug", "aug"),
        description="6-note symmetric - dreamlike, impressionist, Debussy"
    ),
    "chromatic": ScaleInfo(
        name="Chromatic",
        intervals=(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),
        chord_qualities=("any",) * 12,
        description="All 12 tones - atonal passages, tension"
    ),
    "augmented": ScaleInfo(
        name="Augmented (Hexatonic)",
        intervals=(0, 3, 4, 7, 8, 11),
        chord_qualities=("aug", "min", "aug", "min", "aug", "min"),
        description="Alternating m3/m2 - Coltrane, symmetric"
    ),
}

# Exotic Scales (Double Harmonic family)
EXOTIC_SCALES = {
    "double_harmonic": ScaleInfo(
        name="Double Harmonic Major (Byzantine)",
        intervals=(0, 1, 4, 5, 7, 8, 11),
        chord_qualities=("maj", "maj", "dim", "min", "maj", "maj", "dim"),
        description="Middle Eastern, exotic, dramatic - both 7ths raised"
    ),
    "phrygian_dominant": ScaleInfo(
        name="Phrygian Dominant (Spanish Gypsy)",
        intervals=(0, 1, 4, 5, 7, 8, 10),
        chord_qualities=("maj", "dim", "min", "min", "maj", "maj", "min"),
        description="5th mode of harmonic minor - flamenco, Jewish"
    ),
    "hungarian_minor": ScaleInfo(
        name="Hungarian Minor",
        intervals=(0, 2, 3, 6, 7, 8, 11),
        chord_qualities=("min", "dim", "aug", "dim", "maj", "maj", "dim"),
        description="Double harmonic minor - gypsy, dramatic tension"
    ),
    "hungarian_major": ScaleInfo(
        name="Hungarian Major",
        intervals=(0, 3, 4, 6, 7, 9, 10),
        chord_qualities=("maj", "dim", "dim", "min", "min", "maj", "dim"),
        description="Raised 2nd and 6th - Eastern European folk"
    ),
    "enigmatic": ScaleInfo(
        name="Enigmatic",
        intervals=(0, 1, 4, 6, 8, 10, 11),
        chord_qualities=("maj", "aug", "aug", "dim", "maj", "dim", "dim"),
        description="Verdi's exotic scale - mysterious, unresolved"
    ),
    "neapolitan_major": ScaleInfo(
        name="Neapolitan Major",
        intervals=(0, 1, 3, 5, 7, 9, 11),
        chord_qualities=("min", "maj", "aug", "maj", "maj", "dim", "dim"),
        description="Like major with b2 - dark nobility"
    ),
    "neapolitan_minor": ScaleInfo(
        name="Neapolitan Minor",
        intervals=(0, 1, 3, 5, 7, 8, 11),
        chord_qualities=("min", "dim", "aug", "min", "maj", "maj", "dim"),
        description="Harmonic minor with b2 - opera, drama"
    ),
    "persian": ScaleInfo(
        name="Persian",
        intervals=(0, 1, 4, 5, 6, 8, 11),
        chord_qualities=("maj", "maj", "dim", "dim", "aug", "maj", "dim"),
        description="Middle Eastern - exotic chromaticism"
    ),
    "prometheus": ScaleInfo(
        name="Prometheus (Scriabin)",
        intervals=(0, 2, 4, 6, 9, 10),
        chord_qualities=("maj", "maj", "dim", "min", "min", "dim"),
        description="Scriabin's mystic chord basis"
    ),
}

# Bebop Scales (Jazz articulation)
BEBOP_SCALES = {
    "bebop_dominant": ScaleInfo(
        name="Bebop Dominant",
        intervals=(0, 2, 4, 5, 7, 9, 10, 11),
        chord_qualities=("dom",) * 8,
        description="Mixolydian + major 7th passing tone"
    ),
    "bebop_major": ScaleInfo(
        name="Bebop Major",
        intervals=(0, 2, 4, 5, 7, 8, 9, 11),
        chord_qualities=("maj",) * 8,
        description="Major + #5 passing tone"
    ),
    "bebop_minor": ScaleInfo(
        name="Bebop Minor (Dorian)",
        intervals=(0, 2, 3, 4, 5, 7, 9, 10),
        chord_qualities=("min",) * 8,
        description="Dorian + major 3rd passing tone"
    ),
    "bebop_locrian": ScaleInfo(
        name="Bebop Locrian",
        intervals=(0, 1, 3, 5, 6, 8, 10, 11),
        chord_qualities=("dim",) * 8,
        description="Locrian + added natural 7th"
    ),
}

# Modal Variations (Extended Modes)
MODAL_VARIATIONS = {
    "lydian_dominant": ScaleInfo(
        name="Lydian Dominant (Lydian b7)",
        intervals=(0, 2, 4, 6, 7, 9, 10),
        chord_qualities=("maj", "maj", "dim", "dim", "maj", "min", "min"),
        description="4th mode of melodic minor - jazz fusion"
    ),
    "lydian_augmented": ScaleInfo(
        name="Lydian Augmented",
        intervals=(0, 2, 4, 6, 8, 9, 11),
        chord_qualities=("aug", "maj", "dim", "dim", "aug", "min", "dim"),
        description="3rd mode of melodic minor - Pat Metheny"
    ),
    "super_locrian": ScaleInfo(
        name="Super Locrian (Altered Scale)",
        intervals=(0, 1, 3, 4, 6, 8, 10),
        chord_qualities=("dim", "min", "min", "dim", "maj", "maj", "maj"),
        description="7th mode of melodic minor - all alterations"
    ),
    "locrian_natural2": ScaleInfo(
        name="Locrian Natural 2",
        intervals=(0, 2, 3, 5, 6, 8, 10),
        chord_qualities=("dim", "min", "maj", "min", "maj", "maj", "min"),
        description="6th mode of melodic minor - half-diminished"
    ),
    "dorian_b2": ScaleInfo(
        name="Dorian b2 (Phrygian #6)",
        intervals=(0, 1, 3, 5, 7, 9, 10),
        chord_qualities=("min", "maj", "maj", "min", "dim", "maj", "min"),
        description="2nd mode of melodic minor - modal jazz"
    ),
}

# All scales combined
ALL_SCALES = {
    **CHURCH_MODES,
    **MINOR_SCALES,
    **PENTATONIC_SCALES,
    **BLUES_SCALES,
    **SYMMETRIC_SCALES,
    **EXOTIC_SCALES,
    **BEBOP_SCALES,
    **MODAL_VARIATIONS,
    # Aliases
    "major": CHURCH_MODES["ionian"],
    "minor": CHURCH_MODES["aeolian"],
    "natural_minor": CHURCH_MODES["aeolian"],
    "diminished_hw": SYMMETRIC_SCALES["octatonic_hw"],
    "diminished_wh": SYMMETRIC_SCALES["octatonic_wh"],
    "altered": MODAL_VARIATIONS["super_locrian"],
    "spanish_gypsy": EXOTIC_SCALES["phrygian_dominant"],
    "byzantine": EXOTIC_SCALES["double_harmonic"],
}


class ScaleSystem:
    """Scale system providing scale information and operations.
    
    Component: THSC001A
    
    Domain: TH (Theory)
    Category: SC (Scale)
    
    A bounded scale lookup and generation system supporting 40+ scales
    including church modes, exotic scales, and symmetric scales.
    
    Bounds:
        - root_midi: int ∈ [0, 127]
        - scale_name: str ∈ {valid_scale_names} (see list_scales())
        - degree: int ∈ [1, len(scale.intervals)]
    
    Error Codes:
        - 1301: Invalid scale name
        - 1302: Invalid scale degree
    """
    
    def __init__(self):
        """Initialize scale system."""
        self._scales = ALL_SCALES
    
    def get_scale(self, name: str) -> ScaleInfo | None:
        """Get scale information by name.
        
        Args:
            name: Scale name (e.g., "dorian", "harmonic_minor")
        
        Returns:
            ScaleInfo or None if not found
        """
        normalized = name.lower().replace("-", "_").replace(" ", "_")
        return self._scales.get(normalized)
    
    def list_scales(self) -> list[str]:
        """List all available scale names.
        
        Returns:
            List of scale names
        """
        # Return unique names (exclude aliases pointing to same scale)
        seen = set()
        names = []
        for name, info in self._scales.items():
            if id(info) not in seen:
                names.append(name)
                seen.add(id(info))
        return sorted(names)
    
    def get_scale_notes(
        self,
        root_midi: int,
        scale_name: str
    ) -> list[int]:
        """Get MIDI notes for a scale.
        
        Component: THSC001A.get_scale_notes
        
        Args:
            root_midi: MIDI note number of root ∈ [0, 127]
            scale_name: Scale name ∈ {valid_scale_names}
        
        Returns:
            notes: list[int] where:
                - len(notes) = len(scale.intervals)
                - ∀ n ∈ notes: n ∈ [root_midi, root_midi + 11]
        
        Invariants:
            - notes[0] = root_midi
            - ∀ i < j: notes[i] < notes[j] (ascending order)
        
        Raises:
            ValueError: If scale_name unknown (Error 1301)
        """
        scale = self.get_scale(scale_name)
        if not scale:
            raise ValueError(f"Unknown scale: {scale_name}")
        
        return [root_midi + interval for interval in scale.intervals]
    
    def get_chord_quality(
        self,
        scale_name: str,
        degree: int
    ) -> str:
        """Get chord quality for a scale degree.
        
        Component: THSC001A.get_chord_quality
        
        Args:
            scale_name: Scale name ∈ {valid_scale_names}
            degree: Scale degree ∈ [1, len(scale.intervals)]
        
        Returns:
            quality: str ∈ {"maj", "min", "dim", "aug", "dom", "any"}
        
        Raises:
            ValueError: If scale_name unknown (Error 1301)
            ValueError: If degree out of range (Error 1302)
        """
        scale = self.get_scale(scale_name)
        if not scale:
            raise ValueError(f"Unknown scale: {scale_name}")
        
        if not 1 <= degree <= len(scale.chord_qualities):
            raise ValueError(f"Invalid degree {degree} for scale {scale_name}")
        
        return scale.chord_qualities[degree - 1]
    
    def is_note_in_scale(
        self,
        note_midi: int,
        root_midi: int,
        scale_name: str
    ) -> bool:
        """Check if a MIDI note is in a scale.
        
        Args:
            note_midi: MIDI note to check
            root_midi: MIDI note of scale root
            scale_name: Scale name
        
        Returns:
            True if note is in scale
        """
        scale = self.get_scale(scale_name)
        if not scale:
            raise ValueError(f"Unknown scale: {scale_name}")
        
        # Get pitch class relative to root
        pitch_class = (note_midi - root_midi) % 12
        
        return pitch_class in scale.intervals
