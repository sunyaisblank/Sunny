"""Sunny - Functional Harmony Module.

Component: THCH001A
Domain: TH (Theory) | Category: CH (Chord/Harmony)

Advanced harmonic operations including:
- Functional polarity (Tonic/Subdominant/Dominant)
- Negative harmony (axis-based chord mirroring)
- Secondary dominants and applied chords
- Modal interchange and pivot chords
- Augmented sixth chords

Bounds:
    - degree: int ∈ [1, 7]
    - mode: str ∈ {"major", "minor"}
    - octave: int ∈ [2, 6]
    - chord_type: str ∈ {"italian", "french", "german"}

Error Codes:
    - 1310: Invalid scale degree
    - 1311: Invalid mode
    - 1312: Invalid chord type
"""

from __future__ import annotations

import logging
from enum import Enum
from typing import Any, NamedTuple

logger = logging.getLogger("sunny.theory.harmony")


# =============================================================================
# Functional Role Definitions
# =============================================================================

class FunctionalRole(Enum):
    """Harmonic function categories.
    
    Component: THCH001A.FunctionalRole
    """
    
    TONIC = "T"           # Stability, resolution (I, vi, iii)
    SUBDOMINANT = "S"     # Pre-dominant, tension building (IV, ii)
    DOMINANT = "D"        # Maximum tension, demands resolution (V, vii°)


class ChordInfo(NamedTuple):
    """Chord with harmonic metadata.
    
    Component: THCH001A.ChordInfo
    
    Fields:
        root: Note name ∈ {C, C#, Db, D, ..., B}
        quality: Chord quality ∈ {major, minor, diminished, augmented, ...}
        notes: tuple[int, ...] where ∀ n: n ∈ [0, 127]
        function: FunctionalRole | None
        numeral: Roman numeral representation
    """
    
    root: str
    quality: str
    notes: tuple[int, ...]
    function: FunctionalRole | None = None
    numeral: str = ""


# =============================================================================
# Note/Pitch Utilities
# =============================================================================

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
NOTE_NAMES_FLAT = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]

NOTE_TO_PITCH = {
    "C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3,
    "E": 4, "Fb": 4, "E#": 5, "F": 5, "F#": 6, "Gb": 6,
    "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10,
    "B": 11, "Cb": 11, "B#": 0,
}


def pitch_class(note: int | str) -> int:
    """Get pitch class (0-11) from MIDI note or note name.
    
    Component: THCH001A.pitch_class
    
    Args:
        note: MIDI note ∈ [0, 127] or note name ∈ {valid_note_names}
    
    Returns:
        pc: int ∈ [0, 11]
    """
    if isinstance(note, str):
        return NOTE_TO_PITCH.get(note, 0)
    return note % 12


def note_name(pitch: int, prefer_flats: bool = False) -> str:
    """Get note name from pitch class.
    
    Component: THCH001A.note_name
    
    Args:
        pitch: MIDI note or pitch class
        prefer_flats: Use flat names instead of sharps
    
    Returns:
        name: str ∈ {C, C#/Db, D, D#/Eb, E, F, F#/Gb, G, G#/Ab, A, A#/Bb, B}
    """
    pc = pitch % 12
    return NOTE_NAMES_FLAT[pc] if prefer_flats else NOTE_NAMES[pc]


# =============================================================================
# Functional Harmony Analysis
# =============================================================================

# Functional roles by scale degree (1-indexed)
MAJOR_FUNCTIONS = {
    1: FunctionalRole.TONIC,      # I
    2: FunctionalRole.SUBDOMINANT, # ii
    3: FunctionalRole.TONIC,      # iii (tonic substitute)
    4: FunctionalRole.SUBDOMINANT, # IV
    5: FunctionalRole.DOMINANT,   # V
    6: FunctionalRole.TONIC,      # vi (tonic substitute)
    7: FunctionalRole.DOMINANT,   # vii° (dominant substitute)
}

MINOR_FUNCTIONS = {
    1: FunctionalRole.TONIC,      # i
    2: FunctionalRole.SUBDOMINANT, # ii°
    3: FunctionalRole.TONIC,      # III
    4: FunctionalRole.SUBDOMINANT, # iv
    5: FunctionalRole.DOMINANT,   # v or V
    6: FunctionalRole.SUBDOMINANT, # VI
    7: FunctionalRole.DOMINANT,   # VII or vii°
}


class HarmonyModule:
    """Advanced functional harmony operations.
    
    Component: THCH001A
    
    Provides tools for professional-grade harmonic analysis and generation,
    including techniques from the "deep" music theory iceberg.
    
    Bounds:
        - degree: int ∈ [1, 7]
        - target_degree: int ∈ [2, 7] (for secondary dominants)
        - octave: int ∈ [2, 6]
        - mode: str ∈ {"major", "minor"}
        - chord_type (aug6): str ∈ {"italian", "french", "german"}
    
    Error Codes:
        - 1310: Invalid scale degree
        - 1311: Invalid mode
        - 1312: Invalid chord type
        - 1313: Invalid augmented sixth type
    """
    
    # Chord quality intervals (from root)
    CHORD_INTERVALS = {
        "major": (0, 4, 7),
        "minor": (0, 3, 7),
        "diminished": (0, 3, 6),
        "augmented": (0, 4, 8),
        "dominant7": (0, 4, 7, 10),
        "major7": (0, 4, 7, 11),
        "minor7": (0, 3, 7, 10),
        "half_diminished": (0, 3, 6, 10),
        "diminished7": (0, 3, 6, 9),
    }
    
    # ==========================================================================
    # Functional Analysis
    # ==========================================================================
    
    def get_functional_role(
        self,
        degree: int,
        mode: str = "major"
    ) -> FunctionalRole:
        """Get the harmonic function of a scale degree.
        
        Args:
            degree: Scale degree (1-7)
            mode: "major" or "minor"
        
        Returns:
            FunctionalRole enum value
        
        Example:
            >>> harmony.get_functional_role(5, "major")
            FunctionalRole.DOMINANT
        """
        functions = MINOR_FUNCTIONS if mode.lower() == "minor" else MAJOR_FUNCTIONS
        return functions.get(degree, FunctionalRole.TONIC)
    
    def analyze_progression(
        self,
        numerals: list[str],
        mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Analyze a chord progression for functional harmony.
        
        Args:
            numerals: List of Roman numerals
            mode: Key mode ("major" or "minor")
        
        Returns:
            List of analysis dictionaries with function, tension level
        
        Example:
            >>> harmony.analyze_progression(["ii", "V", "I"], "major")
            [
                {"numeral": "ii", "function": "S", "tension": 1},
                {"numeral": "V", "function": "D", "tension": 2},
                {"numeral": "I", "function": "T", "tension": 0},
            ]
        """
        numeral_to_degree = {
            "i": 1, "I": 1, "ii": 2, "II": 2, "iii": 3, "III": 3,
            "iv": 4, "IV": 4, "v": 5, "V": 5, "vi": 6, "VI": 6,
            "vii": 7, "VII": 7,
        }
        
        tension_levels = {
            FunctionalRole.TONIC: 0,
            FunctionalRole.SUBDOMINANT: 1,
            FunctionalRole.DOMINANT: 2,
        }
        
        analysis = []
        for numeral in numerals:
            # Extract base numeral
            base = numeral.lower().rstrip("°o+7654")
            degree = numeral_to_degree.get(base, 1)
            
            function = self.get_functional_role(degree, mode)
            
            analysis.append({
                "numeral": numeral,
                "degree": degree,
                "function": function.value,
                "function_name": function.name.lower(),
                "tension": tension_levels[function],
            })
        
        return analysis
    
    # ==========================================================================
    # Negative Harmony
    # ==========================================================================
    
    def generate_negative_mirror(
        self,
        chord_pitches: list[int],
        axis: int = 0
    ) -> list[int]:
        """Mirror chord pitches around an axis using negative harmony.
        
        Negative harmony (Ernst Levy / Jacob Collier) mirrors pitches
        around the axis between the tonic and dominant (typically between
        Eb and E in C major, which is the axis at pitch class 3.5).
        
        Args:
            chord_pitches: List of MIDI note numbers
            axis: Axis pitch class (default: 0 for C, use 3.5 for C major axis)
        
        Returns:
            List of mirrored MIDI note numbers
        
        Example:
            C major (C-E-G) mirrors to F minor (F-Ab-C) around axis
        """
        mirrored = []
        for pitch in chord_pitches:
            # Mirror around the axis
            # Formula: new_pitch = 2 * axis - old_pitch
            pc = pitch % 12
            octave = pitch // 12
            
            # Mirror the pitch class
            mirrored_pc = (2 * axis - pc) % 12
            
            # Reconstruct with same octave (may need adjustment)
            new_pitch = octave * 12 + int(mirrored_pc)
            
            # Keep in valid MIDI range
            while new_pitch < 0:
                new_pitch += 12
            while new_pitch > 127:
                new_pitch -= 12
            
            mirrored.append(new_pitch)
        
        return sorted(mirrored)
    
    def mirror_chord(
        self,
        root: str,
        quality: str,
        key: str = "C",
        octave: int = 4
    ) -> ChordInfo:
        """Mirror a chord using negative harmony in a given key.
        
        The axis is placed between scale degrees 1 and 5 of the key.
        For C major, this is between Eb and E (axis at 3.5).
        
        Args:
            root: Chord root note name
            quality: Chord quality ("major", "minor", etc.)
            key: Key for the axis calculation
            octave: Octave for output pitches
        
        Returns:
            ChordInfo with mirrored chord
        
        Example:
            >>> harmony.mirror_chord("C", "major", "C", 4)
            ChordInfo(root='F', quality='minor', notes=(65, 68, 72))
        """
        # Get chord pitches
        root_pc = pitch_class(root)
        intervals = self.CHORD_INTERVALS.get(quality, (0, 4, 7))
        
        base_midi = (octave + 1) * 12 + root_pc
        pitches = [base_midi + i for i in intervals]
        
        # Calculate axis for the key
        # Axis is between 3rd and 4th degree (Eb-E in C major)
        key_pc = pitch_class(key)
        axis = key_pc + 3.5  # Between minor 3rd and major 3rd
        
        # Mirror pitches
        mirrored = self.generate_negative_mirror(pitches, axis)
        
        # Determine new root and quality
        new_root_pc = mirrored[0] % 12
        new_root = note_name(new_root_pc, prefer_flats=True)
        
        # Analyze intervals to determine quality
        if len(mirrored) >= 3:
            third_interval = (mirrored[1] - mirrored[0]) % 12
            if third_interval == 3:
                new_quality = "minor"
            elif third_interval == 4:
                new_quality = "major"
            else:
                new_quality = quality  # Preserve original if unclear
        else:
            new_quality = quality
        
        return ChordInfo(
            root=new_root,
            quality=new_quality,
            notes=tuple(mirrored),
        )
    
    def mirror_progression(
        self,
        root: str,
        mode: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Mirror an entire progression using negative harmony.
        
        Args:
            root: Key root
            mode: Key mode
            numerals: Roman numerals to mirror
            octave: Base octave
        
        Returns:
            List of mirrored chord dictionaries
        
        Example:
            ii-V-I in C major → bVII-iv-i (roughly Bb-Fm-Cm)
        """
        # This would integrate with TheoryEngine for full implementation
        # For now, return the conceptual mapping
        
        # Major key negative mappings
        # I → i (C → Cm in axis-based view, but functionally becomes "i" of relative)
        # V → iv
        # IV → v
        # ii → bVII
        # vi → bIII
        # iii → bVI
        # vii° → bII
        
        major_mirror = {
            "I": "i", "i": "I",
            "V": "iv", "v": "IV",
            "IV": "v", "iv": "V",
            "ii": "bVII", "II": "bvii",
            "vi": "bIII", "VI": "biii",
            "iii": "bVI", "III": "bvi",
            "vii": "bII", "VII": "bii",
        }
        
        mirrored = []
        for numeral in numerals:
            # Get base numeral for mapping
            base = numeral.rstrip("°o+7654")
            mirror_numeral = major_mirror.get(base, numeral)
            
            # Preserve extensions
            extension = numeral[len(base):]
            
            mirrored.append({
                "original": numeral,
                "mirrored": mirror_numeral + extension,
            })
        
        return mirrored
    
    # ==========================================================================
    # Secondary Dominants
    # ==========================================================================
    
    def secondary_dominant(
        self,
        target_degree: int,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> ChordInfo:
        """Generate a secondary dominant (V/x) for a target degree.
        
        A secondary dominant is a major chord or dominant 7th built
        a perfect 5th above the target chord's root.
        
        Args:
            target_degree: Scale degree to tonicize (2-7)
            key: Key root note
            mode: Key mode
            octave: Octave for output
        
        Returns:
            ChordInfo for the secondary dominant
        
        Example:
            >>> harmony.secondary_dominant(5, "C", "major")  # V/V
            ChordInfo(root='D', quality='major', notes=(62, 66, 69))
        """
        # Get scale degrees
        key_pc = pitch_class(key)
        
        # Major scale intervals
        if mode.lower() == "major":
            scale_intervals = [0, 2, 4, 5, 7, 9, 11]
        else:
            # Natural minor
            scale_intervals = [0, 2, 3, 5, 7, 8, 10]
        
        # Get target root (0-indexed degree)
        target_interval = scale_intervals[target_degree - 1]
        target_root_pc = (key_pc + target_interval) % 12
        
        # Secondary dominant is a P5 above target
        secondary_root_pc = (target_root_pc + 7) % 12
        secondary_root = note_name(secondary_root_pc)
        
        # Build dominant chord
        base_midi = (octave + 1) * 12 + secondary_root_pc
        notes = tuple(base_midi + i for i in (0, 4, 7))
        
        return ChordInfo(
            root=secondary_root,
            quality="major",
            notes=notes,
            function=FunctionalRole.DOMINANT,
            numeral=f"V/{self._degree_to_numeral(target_degree)}",
        )
    
    def secondary_dominant_seventh(
        self,
        target_degree: int,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> ChordInfo:
        """Generate a secondary dominant 7th chord.
        
        Args:
            target_degree: Scale degree to tonicize
            key: Key root
            mode: Key mode
            octave: Output octave
        
        Returns:
            ChordInfo with dominant 7th chord
        """
        base = self.secondary_dominant(target_degree, key, mode, octave)
        
        # Add the 7th (minor 7th above root)
        seventh = base.notes[0] + 10
        notes = base.notes + (seventh,)
        
        return ChordInfo(
            root=base.root,
            quality="dominant7",
            notes=notes,
            function=FunctionalRole.DOMINANT,
            numeral=f"V7/{self._degree_to_numeral(target_degree)}",
        )
    
    def _degree_to_numeral(self, degree: int) -> str:
        """Convert scale degree to Roman numeral."""
        numerals = ["I", "II", "III", "IV", "V", "VI", "VII"]
        return numerals[degree - 1] if 1 <= degree <= 7 else str(degree)
    
    # ==========================================================================
    # Modal Interchange
    # ==========================================================================
    
    def get_borrowed_chords(
        self,
        key: str,
        from_mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Get chords that can be borrowed from parallel modes.
        
        When in major, common borrowed chords come from parallel minor:
        - iv (minor iv instead of major IV)
        - bVI (flat six major)
        - bVII (flat seven major)
        - bIII (flat three major)
        
        Args:
            key: Key root
            from_mode: Current mode ("major" gets minor borrowings)
        
        Returns:
            List of borrowable chord info
        """
        key_pc = pitch_class(key)
        
        if from_mode.lower() == "major":
            # Borrow from parallel minor
            borrowed = [
                {"numeral": "iv", "interval": 5, "quality": "minor", 
                 "description": "Minor iv - darker subdominant"},
                {"numeral": "bVI", "interval": 8, "quality": "major",
                 "description": "Flat VI - deceptive resolution target"},
                {"numeral": "bVII", "interval": 10, "quality": "major",
                 "description": "Flat VII - Mixolydian color"},
                {"numeral": "bIII", "interval": 3, "quality": "major",
                 "description": "Flat III - relative major sound"},
                {"numeral": "i", "interval": 0, "quality": "minor",
                 "description": "Minor tonic - modal mixture"},
            ]
        else:
            # Borrow from parallel major
            borrowed = [
                {"numeral": "IV", "interval": 5, "quality": "major",
                 "description": "Major IV - brighter subdominant"},
                {"numeral": "V", "interval": 7, "quality": "major",
                 "description": "Major V - stronger dominant (harmonic minor)"},
                {"numeral": "I", "interval": 0, "quality": "major",
                 "description": "Major tonic - Picardy third"},
            ]
        
        # Add root notes
        for chord in borrowed:
            root_pc = (key_pc + chord["interval"]) % 12
            chord["root"] = note_name(root_pc)
        
        return borrowed
    
    def find_pivot_chords(
        self,
        key1: str,
        mode1: str,
        key2: str,
        mode2: str
    ) -> list[dict[str, Any]]:
        """Find chords that exist in both keys for modulation.
        
        Pivot chords are diatonic in both the source and target keys,
        making them ideal for smooth modulations.
        
        Args:
            key1: Source key root
            mode1: Source mode
            key2: Target key root
            mode2: Target mode
        
        Returns:
            List of pivot chords with their functions in each key
        """
        key1_pc = pitch_class(key1)
        key2_pc = pitch_class(key2)
        
        # Build diatonic triads for each key
        major_intervals = [0, 2, 4, 5, 7, 9, 11]
        minor_intervals = [0, 2, 3, 5, 7, 8, 10]
        
        major_qualities = ["maj", "min", "min", "maj", "maj", "min", "dim"]
        minor_qualities = ["min", "dim", "maj", "min", "min", "maj", "maj"]
        
        def get_triads(root_pc, mode):
            intervals = major_intervals if mode == "major" else minor_intervals
            qualities = major_qualities if mode == "major" else minor_qualities
            triads = {}
            for i, (interval, quality) in enumerate(zip(intervals, qualities)):
                chord_root = (root_pc + interval) % 12
                triads[chord_root] = {
                    "degree": i + 1,
                    "quality": quality,
                    "root": note_name(chord_root),
                }
            return triads
        
        triads1 = get_triads(key1_pc, mode1)
        triads2 = get_triads(key2_pc, mode2)
        
        # Find common chords
        pivots = []
        for root_pc, info1 in triads1.items():
            if root_pc in triads2:
                info2 = triads2[root_pc]
                if info1["quality"] == info2["quality"]:
                    pivots.append({
                        "root": info1["root"],
                        "quality": info1["quality"],
                        f"in_{key1}_{mode1}": self._degree_to_numeral(info1["degree"]),
                        f"in_{key2}_{mode2}": self._degree_to_numeral(info2["degree"]),
                    })
        
        return pivots
    
    # ==========================================================================
    # Augmented Sixth Chords
    # ==========================================================================
    
    def augmented_sixth(
        self,
        chord_type: str,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> ChordInfo:
        """Generate an augmented sixth chord.
        
        Augmented sixths are pre-dominant chords that resolve to V.
        The augmented 6th interval (e.g., Ab to F#) creates strong
        chromatic pull to the dominant.
        
        Types:
        - Italian: b6 - 1 - #4 (3 notes)
        - French: b6 - 1 - 2 - #4 (4 notes)
        - German: b6 - 1 - b3 - #4 (4 notes)
        
        Args:
            chord_type: "italian", "french", or "german"
            key: Key root
            mode: Key mode
            octave: Output octave
        
        Returns:
            ChordInfo for the augmented sixth
        
        Example:
            In C major, Italian +6 = Ab-C-F#
        """
        key_pc = pitch_class(key)
        base = (octave + 1) * 12
        
        # Base note is b6 (flat 6th scale degree)
        b6 = base + (key_pc + 8) % 12  # Ab in C
        
        # Tonic
        tonic = base + key_pc  # C in C
        
        # Raised 4th (leading to dominant)
        sharp4 = base + (key_pc + 6) % 12  # F# in C
        
        chord_type = chord_type.lower()
        
        if chord_type == "italian":
            notes = (b6, tonic, sharp4 + 12)  # +12 to create the +6 interval
        elif chord_type == "french":
            # Add the 2nd scale degree
            second = base + (key_pc + 2) % 12  # D in C
            notes = (b6, tonic, second, sharp4 + 12)
        elif chord_type == "german":
            # Add the flat 3rd (enharmonic to #2)
            b3 = base + (key_pc + 3) % 12  # Eb in C
            notes = (b6, tonic, b3, sharp4 + 12)
        else:
            raise ValueError(f"Unknown augmented sixth type: {chord_type}")
        
        return ChordInfo(
            root=note_name((key_pc + 8) % 12, prefer_flats=True),
            quality=f"{chord_type}_augmented_sixth",
            notes=notes,
            function=FunctionalRole.SUBDOMINANT,
            numeral=f"+6 ({chord_type.capitalize()})",
        )
    
    # ==========================================================================
    # Special Chords
    # ==========================================================================
    
    def neapolitan_chord(
        self,
        key: str,
        mode: str = "major",
        octave: int = 4,
        inversion: int = 1
    ) -> ChordInfo:
        """Generate a Neapolitan chord (bII).
        
        The Neapolitan is a major chord built on the lowered 2nd degree,
        typically used in first inversion (bII6) as a pre-dominant.
        
        Args:
            key: Key root
            mode: Key mode
            octave: Output octave
            inversion: 0 = root, 1 = first inversion (common)
        
        Returns:
            ChordInfo for Neapolitan chord
        
        Example:
            In C major/minor, Neapolitan = Db major (Db-F-Ab)
        """
        key_pc = pitch_class(key)
        
        # bII root is a semitone above tonic
        neapolitan_root = (key_pc + 1) % 12
        
        base = (octave + 1) * 12 + neapolitan_root
        
        # Major triad
        if inversion == 0:
            notes = (base, base + 4, base + 7)
        elif inversion == 1:
            # First inversion: 3rd in bass
            notes = (base + 4 - 12, base, base + 7)
        else:
            notes = (base + 7 - 12, base, base + 4)
        
        return ChordInfo(
            root=note_name(neapolitan_root, prefer_flats=True),
            quality="major",
            notes=notes,
            function=FunctionalRole.SUBDOMINANT,
            numeral="bII6" if inversion == 1 else "bII",
        )
    
    def tritone_substitution(
        self,
        dominant_root: str,
        octave: int = 4
    ) -> ChordInfo:
        """Generate a tritone substitution for a dominant chord.
        
        The tritone sub replaces a dominant 7th with another dominant 7th
        whose root is a tritone away. They share the same tritone interval
        (3rd and 7th are swapped).
        
        Args:
            dominant_root: Root of the original dominant chord
            octave: Output octave
        
        Returns:
            ChordInfo for the tritone substitution
        
        Example:
            G7 → Db7 (both contain B and F)
        """
        root_pc = pitch_class(dominant_root)
        
        # Tritone away
        sub_root_pc = (root_pc + 6) % 12
        sub_root = note_name(sub_root_pc, prefer_flats=True)
        
        base = (octave + 1) * 12 + sub_root_pc
        notes = (base, base + 4, base + 7, base + 10)  # Dominant 7th
        
        return ChordInfo(
            root=sub_root,
            quality="dominant7",
            notes=notes,
            function=FunctionalRole.DOMINANT,
            numeral=f"subV7 (for V7 on {dominant_root})",
        )
