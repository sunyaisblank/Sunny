"""Theory Engine - Thin wrapper around C++ backend.

Component: CREN001A
Domain: CR (Core) | Category: EN (Engine)

Provides a Python-friendly interface to the C++ native backend.
All computation is delegated to the native extension.
"""

from __future__ import annotations

from typing import Any, Optional

from sunny.core import (
    NATIVE_AVAILABLE,
    pitch_class,
    transpose,
    invert,
    note_name,
    pitch_octave_to_midi,
    closest_pitch_class_midi,
    euclidean_rhythm,
    NOTE_NAME_TO_PC,
)

# Try to import native backend for advanced features
try:
    import sunny_native
except ImportError:
    sunny_native = None


# Scale intervals (fallback if native not available)
SCALE_INTERVALS = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "harmonic_minor": [0, 2, 3, 5, 7, 8, 11],
    "melodic_minor": [0, 2, 3, 5, 7, 9, 11],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "aeolian": [0, 2, 3, 5, 7, 8, 10],
    "locrian": [0, 1, 3, 5, 6, 8, 10],
    "pentatonic_major": [0, 2, 4, 7, 9],
    "pentatonic_minor": [0, 3, 5, 7, 10],
    "blues": [0, 3, 5, 6, 7, 10],
    "whole_tone": [0, 2, 4, 6, 8, 10],
    "chromatic": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
}

# Chord intervals
CHORD_INTERVALS = {
    "major": [0, 4, 7],
    "minor": [0, 3, 7],
    "diminished": [0, 3, 6],
    "augmented": [0, 4, 8],
    "sus2": [0, 2, 7],
    "sus4": [0, 5, 7],
    "7": [0, 4, 7, 10],
    "maj7": [0, 4, 7, 11],
    "m7": [0, 3, 7, 10],
    "dim7": [0, 3, 6, 9],
    "m7b5": [0, 3, 6, 10],
}

# Roman numeral to degree mapping
NUMERAL_TO_DEGREE = {
    "I": 0, "i": 0,
    "II": 1, "ii": 1,
    "III": 2, "iii": 2,
    "IV": 3, "iv": 3,
    "V": 4, "v": 4,
    "VI": 5, "vi": 5,
    "VII": 6, "vii": 6,
}

# Harmonic functions by degree (0-indexed)
HARMONIC_FUNCTIONS = {
    0: "T",  # I - Tonic
    1: "S",  # ii - Subdominant
    2: "T",  # iii - Tonic substitute
    3: "S",  # IV - Subdominant
    4: "D",  # V - Dominant
    5: "T",  # vi - Tonic substitute
    6: "D",  # vii° - Dominant substitute
}


class TheoryEngine:
    """High-level theory engine with Python fallback.

    This is a thin wrapper that delegates to the C++ native backend
    when available, with pure Python fallback.
    """

    def __init__(self) -> None:
        """Initialize the theory engine."""
        self._native = sunny_native if NATIVE_AVAILABLE else None

    @property
    def is_native(self) -> bool:
        """Check if using native backend."""
        return self._native is not None

    def get_scale_notes(
        self,
        root: str,
        mode: str,
        octave: int = 4
    ) -> list[int]:
        """Get MIDI notes for a scale.

        Args:
            root: Root note name (e.g., "C", "F#")
            mode: Scale mode name
            octave: Base octave

        Returns:
            List of MIDI note numbers
        """
        root_pc = NOTE_NAME_TO_PC.get(root, 0)
        intervals = SCALE_INTERVALS.get(mode.lower(), SCALE_INTERVALS["major"])

        if self._native:
            return self._native.generate_scale_notes(root_pc, intervals, octave)

        # Fallback
        base_midi = pitch_octave_to_midi(root_pc, octave)
        return [base_midi + interval for interval in intervals]

    def generate_progression(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate a chord progression from Roman numerals.

        Args:
            root: Key root note
            scale: Scale name
            numerals: List of Roman numerals
            octave: Base octave

        Returns:
            List of chord dictionaries
        """
        root_pc = NOTE_NAME_TO_PC.get(root, 0)
        scale_intervals = SCALE_INTERVALS.get(scale.lower(), SCALE_INTERVALS["major"])

        chords = []
        for numeral in numerals:
            chord = self._generate_chord_from_numeral(
                numeral, root_pc, scale_intervals, octave
            )
            if chord:
                chords.append(chord)

        return chords

    def _generate_chord_from_numeral(
        self,
        numeral: str,
        key_root: int,
        scale_intervals: list[int],
        octave: int
    ) -> dict[str, Any] | None:
        """Generate a chord from a Roman numeral."""
        # Strip modifiers for degree lookup
        base_numeral = numeral.rstrip("°o+7")

        degree = NUMERAL_TO_DEGREE.get(base_numeral)
        if degree is None:
            return None

        # Determine chord root from scale degree
        if degree < len(scale_intervals):
            chord_root_pc = (key_root + scale_intervals[degree]) % 12
        else:
            return None

        # Determine quality
        is_upper = base_numeral[0].isupper() if base_numeral else True
        has_dim = "°" in numeral or "o" in numeral
        has_7 = "7" in numeral

        if has_dim:
            quality = "dim7" if has_7 else "diminished"
        elif is_upper:
            quality = "7" if has_7 else "major"
        else:
            quality = "m7" if has_7 else "minor"

        # Get chord intervals
        intervals = CHORD_INTERVALS.get(quality, CHORD_INTERVALS["major"])

        # Generate MIDI notes
        base_midi = pitch_octave_to_midi(chord_root_pc, octave)
        notes = [base_midi + i for i in intervals if base_midi + i <= 127]

        return {
            "numeral": numeral,
            "root": note_name(chord_root_pc),
            "quality": quality,
            "notes": notes,
        }

    def generate_progression_voiced(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate a progression with voice leading."""
        chords = self.generate_progression(root, scale, numerals, octave)

        if not chords or not self._native:
            return chords

        # Apply voice leading between successive chords
        for i in range(1, len(chords)):
            source = chords[i - 1]["notes"]
            target_pcs = [n % 12 for n in chords[i]["notes"]]

            try:
                result = self._native.voice_lead_nearest_tone(
                    source, target_pcs, True, False, False
                )
                chords[i]["voiced_notes"] = list(result.voiced_notes)
                chords[i]["notes"] = list(result.voiced_notes)
            except Exception:
                chords[i]["voiced_notes"] = chords[i]["notes"]

        return chords

    def analyze_progression_functions(
        self,
        numerals: list[str],
        mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Analyze harmonic functions of a progression."""
        result = []
        for numeral in numerals:
            base = numeral.rstrip("°o+7")
            degree = NUMERAL_TO_DEGREE.get(base, 0)
            func = HARMONIC_FUNCTIONS.get(degree, "T")

            tension = 0
            if func == "D":
                tension = 2
            elif func == "S":
                tension = 1

            result.append({
                "numeral": numeral,
                "function": func,
                "tension": tension,
            })

        return result

    def generate_negative_progression(
        self,
        root: str,
        scale: str,
        numerals: list[str]
    ) -> list[dict[str, Any]]:
        """Generate negative harmony version of a progression."""
        root_pc = NOTE_NAME_TO_PC.get(root, 0)
        result = []

        for numeral in numerals:
            base = numeral.rstrip("°o+7")
            degree = NUMERAL_TO_DEGREE.get(base, 0)

            # Get chord pitch classes
            scale_intervals = SCALE_INTERVALS.get(scale.lower(), SCALE_INTERVALS["major"])
            if degree < len(scale_intervals):
                chord_root = (root_pc + scale_intervals[degree]) % 12
            else:
                chord_root = root_pc

            # Apply negative harmony
            if self._native:
                original_pcs = {chord_root, (chord_root + 4) % 12, (chord_root + 7) % 12}
                neg_pcs = self._native.negative_harmony(original_pcs, root_pc)
                neg_root = min(neg_pcs) if neg_pcs else chord_root
            else:
                # Fallback: simple inversion
                axis = (root_pc + 4) % 12
                neg_root = (2 * axis - chord_root) % 12

            result.append({
                "original": numeral,
                "negative_root": note_name(neg_root),
            })

        return result

    def add_secondary_dominant(
        self,
        progression: list[str],
        before_numeral: str
    ) -> list[str]:
        """Add secondary dominant before a target chord."""
        result = []
        for numeral in progression:
            if numeral == before_numeral:
                # Add V/x before x
                result.append(f"V/{before_numeral}")
            result.append(numeral)
        return result

    def get_borrowed_chords(
        self,
        key: str,
        mode: str = "major"
    ) -> list[dict[str, Any]]:
        """Get chords available through modal interchange."""
        if mode.lower() == "major":
            return [
                {"numeral": "iv", "source": "minor", "description": "Minor iv"},
                {"numeral": "bVI", "source": "minor", "description": "Flat VI"},
                {"numeral": "bVII", "source": "minor", "description": "Flat VII"},
                {"numeral": "bIII", "source": "minor", "description": "Flat III"},
            ]
        else:
            return [
                {"numeral": "IV", "source": "major", "description": "Major IV"},
                {"numeral": "V", "source": "major", "description": "Major V"},
            ]

    def create_cadence(
        self,
        cadence_type: str,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Create a specific cadence type."""
        cadences = {
            "perfect_authentic": ["V", "I"],
            "imperfect_authentic": ["V", "I"],
            "half": ["I", "V"],
            "plagal": ["IV", "I"],
            "deceptive": ["V", "vi"],
            "picardy": ["iv", "I"],
        }

        numerals = cadences.get(cadence_type, ["V", "I"])
        return self.generate_progression(key, mode, numerals, octave)

    def list_cadence_types(self) -> list[dict[str, Any]]:
        """List all available cadence types."""
        return [
            {"type": "perfect_authentic", "numerals": ["V", "I"], "emotion": "conclusive"},
            {"type": "imperfect_authentic", "numerals": ["V", "I"], "emotion": "conclusive"},
            {"type": "half", "numerals": ["I", "V"], "emotion": "suspenseful"},
            {"type": "plagal", "numerals": ["IV", "I"], "emotion": "peaceful"},
            {"type": "deceptive", "numerals": ["V", "vi"], "emotion": "surprising"},
            {"type": "picardy", "numerals": ["iv", "I"], "emotion": "hopeful"},
        ]

    def generate_melody(
        self,
        root: str,
        scale: str,
        length: int = 8,
        octave: int = 5,
        chord_tones: Optional[list[int]] = None,
        contour: str = "arch"
    ) -> list[dict[str, Any]]:
        """Generate a scale-aware melody."""
        scale_notes = self.get_scale_notes(root, scale, octave)

        if contour == "arch":
            # Rise then fall
            mid = length // 2
            indices = list(range(mid)) + list(range(mid, -1, -1))
        elif contour == "ascending":
            indices = list(range(length))
        elif contour == "descending":
            indices = list(range(length - 1, -1, -1))
        else:
            indices = list(range(length))

        melody = []
        for i, idx in enumerate(indices[:length]):
            note_idx = idx % len(scale_notes)
            melody.append({
                "pitch": scale_notes[note_idx],
                "start_time": float(i),
                "duration": 1.0,
                "velocity": 100,
            })

        return melody

    def get_available_scales(self) -> list[str]:
        """Get list of available scale names."""
        return sorted(SCALE_INTERVALS.keys())

    def get_scale_info(self, scale_name: str) -> dict[str, Any] | None:
        """Get detailed information about a scale."""
        intervals = SCALE_INTERVALS.get(scale_name.lower())
        if intervals is None:
            return None

        return {
            "name": scale_name,
            "intervals": intervals,
            "note_count": len(intervals),
        }


# Singleton instance
_engine: TheoryEngine | None = None


def get_engine() -> TheoryEngine:
    """Get the shared theory engine instance."""
    global _engine
    if _engine is None:
        _engine = TheoryEngine()
    return _engine
