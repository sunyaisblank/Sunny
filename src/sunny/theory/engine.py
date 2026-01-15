"""Sunny - Theory Engine.

Component: THEN001A
Domain: TH (Theory) | Category: EN (Engine)

Core music theory engine providing scale awareness, chord
progression generation, and harmonic analysis.

Bounds:
    - root: str ∈ {valid_note_names}
    - mode/scale_name: str ∈ {valid_scale_names}
    - octave: int ∈ [0, 9]
    - numerals: list[str] where len ≤ 32

Error Codes:
    - 1300: Invalid root note
    - 1301: Invalid mode/scale name
    - 1302: Invalid octave
    - 1303: Invalid Roman numeral
"""

from __future__ import annotations

import logging
import random
from typing import Any

logger = logging.getLogger("sunny.theory")

# Try to import music21, provide fallback if not available
try:
    from music21 import (
        chord as m21_chord,
        key as m21_key,
        pitch as m21_pitch,
        roman as m21_roman,
        scale as m21_scale,
    )
    MUSIC21_AVAILABLE = True
except ImportError:
    MUSIC21_AVAILABLE = False
    logger.warning("music21 not available - using fallback theory engine")

# Import our advanced modules
from sunny.theory.harmony import HarmonyModule, FunctionalRole, ChordInfo
from sunny.theory.voiceleading import VoiceLeadingEngine
from sunny.theory.cadences import CadenceEngine, CadenceType
from sunny.theory.scales import ScaleSystem, ALL_SCALES


class TheoryEngine:
    """Music theory engine for scale-aware composition.
    
    Component: THEN001A
    
    Provides:
    - Scale/mode note generation (40+ scales including exotic/symmetric)
    - Chord progression from Roman numeral analysis
    - Functional harmony (T/S/D classification)
    - Negative harmony and secondary dominants
    - Voice leading with nearest-tone algorithm
    - Cadence generation and detection
    - Note quantization to scale degrees
    - Chord voicing and inversions
    
    Bounds:
        - root: str ∈ {C, C#, Db, D, ..., B}
        - mode: str ∈ {ionian, dorian, phrygian, ...} (40+ scales)
        - octave: int ∈ [0, 9]
        - numerals: list[str] where len ≤ 32
    
    Error Codes:
        - 1300: Invalid root note
        - 1301: Invalid mode/scale name
        - 1302: Invalid octave range
        - 1303: Invalid Roman numeral
    """
    
    def __init__(self):
        """Initialize the theory engine with component modules."""
        self.harmony = HarmonyModule()
        self.voice_leading = VoiceLeadingEngine()
        self.cadence = CadenceEngine()
        self.scale_system = ScaleSystem()
    
    # Mapping of mode names to music21 scale classes
    MODE_CLASSES = {
        "ionian": "MajorScale",
        "major": "MajorScale",
        "dorian": "DorianScale",
        "phrygian": "PhrygianScale",
        "lydian": "LydianScale",
        "mixolydian": "MixolydianScale",
        "aeolian": "MinorScale",
        "minor": "MinorScale",
        "locrian": "LocrianScale",
        # Additional scales
        "harmonic_minor": "HarmonicMinorScale",
        "melodic_minor": "MelodicMinorScale",
    }
    
    # Fallback interval patterns (semitones from root)
    MODE_INTERVALS = {
        "ionian": [0, 2, 4, 5, 7, 9, 11],
        "major": [0, 2, 4, 5, 7, 9, 11],
        "dorian": [0, 2, 3, 5, 7, 9, 10],
        "phrygian": [0, 1, 3, 5, 7, 8, 10],
        "lydian": [0, 2, 4, 6, 7, 9, 11],
        "mixolydian": [0, 2, 4, 5, 7, 9, 10],
        "aeolian": [0, 2, 3, 5, 7, 8, 10],
        "minor": [0, 2, 3, 5, 7, 8, 10],
        "locrian": [0, 1, 3, 5, 6, 8, 10],
        "harmonic_minor": [0, 2, 3, 5, 7, 8, 11],
        "melodic_minor": [0, 2, 3, 5, 7, 9, 11],
    }
    
    # Roman numeral to diatonic degree mapping
    NUMERAL_DEGREES = {
        "i": 1, "I": 1,
        "ii": 2, "II": 2,
        "iii": 3, "III": 3,
        "iv": 4, "IV": 4,
        "v": 5, "V": 5,
        "vi": 6, "VI": 6,
        "vii": 7, "VII": 7,
    }
    
    # Note name to MIDI offset mapping
    NOTE_OFFSETS = {
        "C": 0, "C#": 1, "Db": 1,
        "D": 2, "D#": 3, "Eb": 3,
        "E": 4, "Fb": 4, "E#": 5,
        "F": 5, "F#": 6, "Gb": 6,
        "G": 7, "G#": 8, "Ab": 8,
        "A": 9, "A#": 10, "Bb": 10,
        "B": 11, "Cb": 11, "B#": 0,
    }
    
    def get_scale_notes(
        self,
        root: str,
        mode: str,
        octave: int = 4
    ) -> list[int]:
        """Get MIDI note numbers for a scale.
        
        Args:
            root: Root note (e.g., "C", "F#", "Bb")
            mode: Scale mode (e.g., "ionian", "dorian", "phrygian")
            octave: Base octave (default: 4, middle C)
        
        Returns:
            List of 7 MIDI note numbers for scale degrees 1-7
        
        Raises:
            ValueError: If mode is not recognized
        """
        mode_lower = mode.lower().replace("-", "_").replace(" ", "_")
        
        if mode_lower not in self.MODE_INTERVALS:
            raise ValueError(
                f"Unknown mode: {mode}. "
                f"Available: {', '.join(self.MODE_INTERVALS.keys())}"
            )
        
        # Get root MIDI note
        root_offset = self._parse_note_name(root)
        root_midi = (octave + 1) * 12 + root_offset
        
        if MUSIC21_AVAILABLE:
            return self._get_scale_notes_m21(root, mode_lower, octave)
        else:
            # Fallback: use interval patterns
            intervals = self.MODE_INTERVALS[mode_lower]
            return [root_midi + interval for interval in intervals]
    
    def _get_scale_notes_m21(
        self,
        root: str,
        mode: str,
        octave: int
    ) -> list[int]:
        """Get scale notes using music21.
        
        Args:
            root: Root note name
            mode: Normalized mode name
            octave: Base octave
        
        Returns:
            List of MIDI note numbers
        """
        scale_class_name = self.MODE_CLASSES.get(mode, "MajorScale")
        scale_class = getattr(m21_scale, scale_class_name)
        
        sc = scale_class(root)
        pitches = sc.getPitches(f"{root}{octave}", f"{root}{octave + 1}")
        
        # Return only first 7 degrees (exclude octave)
        return [int(p.midi) for p in pitches[:7]]
    
    def generate_progression(
        self,
        root: str,
        scale_name: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate chord progression from Roman numerals.
        
        Args:
            root: Key root note (e.g., "C", "F#")
            scale_name: Scale/mode (e.g., "major", "minor", "dorian")
            numerals: Roman numeral sequence (e.g., ["ii", "V", "I"])
            octave: Base octave for voicings
        
        Returns:
            List of chord dictionaries containing:
            - numeral: The Roman numeral
            - root: Chord root note name
            - quality: Chord quality (major, minor, diminished)
            - notes: List of MIDI note numbers
        
        Example:
            >>> engine.generate_progression("C", "major", ["ii", "V", "I"])
            [
                {"numeral": "ii", "root": "D", "quality": "minor", "notes": [62, 65, 69]},
                {"numeral": "V", "root": "G", "quality": "major", "notes": [67, 71, 74]},
                {"numeral": "I", "root": "C", "quality": "major", "notes": [60, 64, 67]},
            ]
        """
        if MUSIC21_AVAILABLE:
            return self._generate_progression_m21(root, scale_name, numerals, octave)
        else:
            return self._generate_progression_fallback(root, scale_name, numerals, octave)
    
    def _generate_progression_m21(
        self,
        root: str,
        scale_name: str,
        numerals: list[str],
        octave: int
    ) -> list[dict[str, Any]]:
        """Generate progression using music21.
        
        Args:
            root: Key root
            scale_name: Scale name
            numerals: Roman numerals
            octave: Base octave
        
        Returns:
            Chord dictionaries
        """
        # Determine key mode
        mode_lower = scale_name.lower()
        if mode_lower in ("minor", "aeolian"):
            key_obj = m21_key.Key(root, "minor")
        else:
            key_obj = m21_key.Key(root, "major")
        
        progression = []
        
        for numeral in numerals:
            # Parse roman numeral relative to key
            try:
                rn = m21_roman.RomanNumeral(numeral, key_obj)
                chord_obj = rn.chord
                
                # Transpose to desired octave
                root_pitch = chord_obj.root()
                target_octave = octave
                octave_shift = target_octave - root_pitch.octave
                
                chord_obj = chord_obj.transpose(octave_shift * 12)
                
                progression.append({
                    "numeral": numeral,
                    "root": rn.root().name,
                    "quality": rn.quality,
                    "notes": [int(p.midi) for p in chord_obj.pitches],
                })
            except Exception as e:
                logger.warning(f"Could not parse numeral '{numeral}': {e}")
                # Skip invalid numerals
                continue
        
        return progression
    
    def _generate_progression_fallback(
        self,
        root: str,
        scale_name: str,
        numerals: list[str],
        octave: int
    ) -> list[dict[str, Any]]:
        """Generate progression using fallback logic.
        
        Args:
            root: Key root
            scale_name: Scale name
            numerals: Roman numerals
            octave: Base octave
        
        Returns:
            Chord dictionaries
        """
        # Get scale notes
        scale_notes = self.get_scale_notes(root, scale_name, octave)
        
        # Determine chord qualities based on scale degree
        mode_lower = scale_name.lower()
        
        # Default major scale chord qualities (I ii iii IV V vi vii°)
        major_qualities = {
            1: ("major", [0, 4, 7]),
            2: ("minor", [0, 3, 7]),
            3: ("minor", [0, 3, 7]),
            4: ("major", [0, 4, 7]),
            5: ("major", [0, 4, 7]),
            6: ("minor", [0, 3, 7]),
            7: ("diminished", [0, 3, 6]),
        }
        
        # Minor scale chord qualities (i ii° III iv v VI VII)
        minor_qualities = {
            1: ("minor", [0, 3, 7]),
            2: ("diminished", [0, 3, 6]),
            3: ("major", [0, 4, 7]),
            4: ("minor", [0, 3, 7]),
            5: ("minor", [0, 3, 7]),
            6: ("major", [0, 4, 7]),
            7: ("major", [0, 4, 7]),
        }
        
        qualities = minor_qualities if mode_lower in ("minor", "aeolian") else major_qualities
        
        progression = []
        note_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        
        for numeral in numerals:
            # Extract base numeral
            base_numeral = numeral.strip("°+7").lower()
            
            if base_numeral not in self.NUMERAL_DEGREES:
                logger.warning(f"Unknown numeral: {numeral}")
                continue
            
            degree = self.NUMERAL_DEGREES[base_numeral]
            root_note = scale_notes[degree - 1]
            
            quality, intervals = qualities.get(degree, ("major", [0, 4, 7]))
            
            # Check for 7th chord
            if "7" in numeral:
                intervals = intervals + [10 if numeral.islower() else 11]
            
            chord_notes = [root_note + interval for interval in intervals]
            chord_root_name = note_names[root_note % 12]
            
            progression.append({
                "numeral": numeral,
                "root": chord_root_name,
                "quality": quality,
                "notes": chord_notes,
            })
        
        return progression
    
    def quantize_to_scale(
        self,
        notes: list[int],
        root: str,
        scale_name: str
    ) -> list[int]:
        """Snap notes to nearest scale degrees.
        
        Args:
            notes: List of MIDI note numbers
            root: Scale root note
            scale_name: Scale/mode name
        
        Returns:
            List of quantized MIDI note numbers
        """
        # Get all scale notes across octaves
        mode_lower = scale_name.lower()
        intervals = self.MODE_INTERVALS.get(mode_lower, self.MODE_INTERVALS["major"])
        root_offset = self._parse_note_name(root)
        
        # Build set of all valid scale pitches
        scale_pitches = set()
        for octave in range(11):  # MIDI octaves 0-10
            base = octave * 12 + root_offset
            for interval in intervals:
                pitch = base + interval
                if 0 <= pitch <= 127:
                    scale_pitches.add(pitch)
        
        # Quantize each note to nearest scale pitch
        quantized = []
        sorted_pitches = sorted(scale_pitches)
        
        for note in notes:
            # Find nearest scale pitch
            closest = min(sorted_pitches, key=lambda x: abs(x - note))
            quantized.append(closest)
        
        return quantized
    
    def _parse_note_name(self, name: str) -> int:
        """Parse note name to MIDI offset (0-11).
        
        Args:
            name: Note name (e.g., "C", "F#", "Bb")
        
        Returns:
            MIDI offset (0-11)
        
        Raises:
            ValueError: If note name is invalid
        """
        if not name:
            raise ValueError("Empty note name")
        
        # Normalize name
        name = name.strip()
        
        # Handle enharmonic equivalents
        if name in self.NOTE_OFFSETS:
            return self.NOTE_OFFSETS[name]
        
        # Try with just first character
        base = name[0].upper()
        modifier = name[1:].lower() if len(name) > 1 else ""
        
        base_offset = self.NOTE_OFFSETS.get(base)
        if base_offset is None:
            raise ValueError(f"Invalid note name: {name}")
        
        # Apply modifier
        if modifier in ("#", "sharp"):
            return (base_offset + 1) % 12
        elif modifier in ("b", "flat"):
            return (base_offset - 1) % 12
        
        return base_offset
    
    # ==========================================================================
    # Advanced Harmony Methods (delegating to HarmonyModule)
    # ==========================================================================
    
    def analyze_progression_functions(
        self,
        progression: list[str],
        mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Analyze functional harmony of a progression.
        
        Args:
            progression: List of Roman numerals
            mode: "major" or "minor"
        
        Returns:
            List of analysis dicts with function (T/S/D) and tension levels
        """
        return self.harmony.analyze_progression(progression, mode)
    
    def generate_negative_progression(
        self,
        root: str,
        scale_name: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate the negative (mirror) version of a progression.
        
        Negative harmony mirrors chords around the tonic-dominant axis,
        transforming brightness to darkness while maintaining function.
        
        Args:
            root: Key root note
            scale_name: Scale/mode name
            numerals: Roman numerals to mirror
            octave: Base octave
        
        Returns:
            List of mirrored chord dictionaries
        """
        mirrored = self.harmony.mirror_progression(root, scale_name, numerals)
        return mirrored
    
    def add_secondary_dominant(
        self,
        progression: list[str],
        before_numeral: str,
        key: str = "C",
        mode: str = "major"
    ) -> list[str]:
        """Insert a secondary dominant before a target chord.
        
        Args:
            progression: Original progression
            before_numeral: Target chord to approach
            key: Key root
            mode: Major or minor
        
        Returns:
            New progression with V/X inserted
        """
        result = []
        for numeral in progression:
            if numeral.upper().strip("7") == before_numeral.upper().strip("7"):
                # Insert V/X before the target
                result.append(f"V/{before_numeral}")
            result.append(numeral)
        return result
    
    def get_borrowed_chords(
        self,
        key: str,
        mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Get chords available through modal interchange.
        
        Args:
            key: Key root note
            mode: Current mode
        
        Returns:
            List of borrowed chord info
        """
        return self.harmony.get_borrowed_chords(key, mode)
    
    # ==========================================================================
    # Voice Leading Methods (delegating to VoiceLeadingEngine)
    # ==========================================================================
    
    def generate_progression_voiced(
        self,
        root: str,
        scale_name: str,
        numerals: list[str],
        octave: int = 4,
        num_voices: int = 4
    ) -> list[dict[str, Any]]:
        """Generate progression with proper voice leading.
        
        Uses nearest-tone algorithm for smooth voice transitions.
        
        Args:
            root: Key root note
            scale_name: Scale/mode name
            numerals: Roman numeral sequence
            octave: Base octave
            num_voices: Number of voices (default 4)
        
        Returns:
            Progression with smoothly voiced chords
        """
        # Generate basic progression
        progression = self.generate_progression(root, scale_name, numerals, octave)
        
        if not progression:
            return []
        
        # Extract chord notes
        chords = [chord["notes"] for chord in progression]
        
        # Apply smooth voice leading
        voiced = self.voice_leading.generate_smooth_progression(
            chords,
            initial_voicing=None
        )
        
        # Merge back into progression
        for i, chord in enumerate(progression):
            chord["voiced_notes"] = voiced[i]
        
        return progression
    
    # ==========================================================================
    # Cadence Methods (delegating to CadenceEngine)
    # ==========================================================================
    
    def create_cadence(
        self,
        cadence_type: str,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Create a specific cadence type.
        
        Args:
            cadence_type: Type of cadence (perfect, plagal, deceptive, etc.)
            key: Key root note
            mode: Major or minor
            octave: Base octave
        
        Returns:
            List of chord dictionaries forming the cadence
        """
        return self.cadence.create_cadence(cadence_type, key, mode, octave)
    
    def list_cadence_types(self) -> list[dict[str, str]]:
        """List all available cadence types.
        
        Returns:
            List of cadence type information
        """
        return self.cadence.list_cadence_types()
    
    # ==========================================================================
    # Melody Generation (NEW)
    # ==========================================================================
    
    def generate_melody(
        self,
        root: str,
        scale_name: str,
        length: int = 8,
        octave: int = 5,
        rhythm_pattern: list[float] | None = None,
        contour: str = "arch"
    ) -> list[dict[str, Any]]:
        """Generate a scale-aware melody.
        
        Args:
            root: Scale root note (e.g., "C", "F#")
            scale_name: Scale/mode name
            length: Number of notes to generate
            octave: Base octave for melody
            rhythm_pattern: Optional rhythm durations (defaults to quarter notes)
            contour: Melodic contour ("arch", "descending", "ascending", "wave")
        
        Returns:
            List of note events with pitch, duration, and beat position
        """
        # Get scale notes
        scale_notes = self.get_scale_notes(root, scale_name, octave)
        
        # Add notes from octave above for melodic range
        scale_notes_extended = scale_notes + [n + 12 for n in scale_notes[:4]]
        
        # Default rhythm: quarter notes
        if rhythm_pattern is None:
            rhythm_pattern = [1.0] * length
        
        # Ensure rhythm pattern matches length
        while len(rhythm_pattern) < length:
            rhythm_pattern = rhythm_pattern + rhythm_pattern
        rhythm_pattern = rhythm_pattern[:length]
        
        # Generate contour
        indices = self._generate_contour(contour, length, len(scale_notes_extended) - 1)
        
        # Build melody
        melody = []
        current_beat = 0.0
        
        for i, idx in enumerate(indices):
            pitch = scale_notes_extended[idx]
            duration = rhythm_pattern[i % len(rhythm_pattern)]
            
            melody.append({
                "pitch": pitch,
                "start_time": current_beat,
                "duration": duration * 0.9,  # Slight articulation gap
                "velocity": 80 + random.randint(-10, 20),  # Natural dynamics
            })
            current_beat += duration
        
        return melody
    
    def _generate_contour(
        self,
        contour: str,
        length: int,
        max_index: int
    ) -> list[int]:
        """Generate scale degree indices based on contour shape.
        
        Args:
            contour: Shape type
            length: Number of notes
            max_index: Maximum scale index available
        
        Returns:
            List of scale degree indices
        """
        half = length // 2
        
        if contour == "arch":
            # Rise to middle, then fall
            rise = [min(i, max_index) for i in range(half + 1)]
            fall = list(reversed(rise))
            indices = rise + fall[1:]
        
        elif contour == "ascending":
            step = max_index / max(1, length - 1)
            indices = [min(int(i * step), max_index) for i in range(length)]
        
        elif contour == "descending":
            step = max_index / max(1, length - 1)
            indices = [max_index - min(int(i * step), max_index) for i in range(length)]
        
        elif contour == "wave":
            import math
            indices = [
                int((math.sin(i * math.pi / (length / 2)) + 1) * max_index / 2)
                for i in range(length)
            ]
        
        else:
            # Random walk
            indices = [max_index // 2]
            for _ in range(length - 1):
                step = random.choice([-2, -1, 0, 1, 2])
                new_idx = max(0, min(max_index, indices[-1] + step))
                indices.append(new_idx)
        
        # Ensure correct length
        while len(indices) < length:
            indices.append(indices[-1] if indices else 0)
        
        return indices[:length]
    
    # ==========================================================================
    # Advanced Scale Access
    # ==========================================================================
    
    def get_available_scales(self) -> list[str]:
        """Get list of all available scale names.
        
        Returns:
            List of scale/mode names
        """
        return list(ALL_SCALES.keys())
    
    def get_scale_info(self, scale_name: str) -> dict[str, Any] | None:
        """Get detailed information about a scale.
        
        Args:
            scale_name: Name of the scale
        
        Returns:
            Dict with scale intervals, chord qualities, and description
        """
        scale = ALL_SCALES.get(scale_name.lower())
        if scale is None:
            return None
        
        return {
            "name": scale.name,
            "intervals": list(scale.intervals),
            "chord_qualities": list(scale.chord_qualities),
            "description": scale.description,
        }

