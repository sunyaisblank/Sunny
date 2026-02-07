"""Theory Engine - Thin wrapper around C++ backend.

Component: CREN001A
Domain: CR (Core) | Category: EN (Engine)

Provides a Python-friendly interface to the C++ native backend.
All computation is delegated to the native extension.

The C++ backend (sunny_native) is required. If unavailable,
ImportError is raised on construction.
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

# Roman numeral to degree mapping (used by Python-level helpers)
NUMERAL_TO_DEGREE = {
    "I": 0, "i": 0,
    "II": 1, "ii": 1,
    "III": 2, "iii": 2,
    "IV": 3, "iv": 3,
    "V": 4, "v": 4,
    "VI": 5, "vi": 5,
    "VII": 6, "vii": 6,
}

# Harmonic functions by degree (used by analyze_progression_functions)
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
    """High-level theory engine backed by sunny_native.

    Requires the C++ native backend. Raises ImportError if unavailable.
    """

    def __init__(self) -> None:
        """Initialize the theory engine.

        Raises:
            ImportError: If sunny_native is not available.
        """
        if not NATIVE_AVAILABLE:
            raise ImportError(
                "sunny_native C++ backend is required. "
                "Build the project with -DSUNNY_BUILD_PYTHON_BINDINGS=ON."
            )
        import sunny_native
        self._native = sunny_native

    @property
    def is_native(self) -> bool:
        """Always True — native backend is required."""
        return True

    def get_scale_notes(
        self,
        root: str,
        mode: str,
        octave: int = 4
    ) -> list[int]:
        """Get MIDI notes for a scale."""
        root_pc = NOTE_NAME_TO_PC.get(root, 0)
        return self._native.generate_scale_notes(root_pc, mode, octave)

    def generate_progression(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate a chord progression from Roman numerals."""
        root_pc = NOTE_NAME_TO_PC.get(root, 0)

        chords = []
        for numeral in numerals:
            try:
                voicing = self._native.generate_chord_from_numeral(
                    numeral, root_pc, scale, octave
                )
                notes = list(voicing.notes) if voicing else []
                quality = voicing.quality if voicing else "unknown"
                chords.append({
                    "numeral": numeral,
                    "root": note_name(notes[0] % 12 if notes else root_pc),
                    "quality": quality,
                    "notes": notes,
                })
            except Exception:
                pass

        return chords

    def generate_progression_voiced(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate a progression with voice leading."""
        chords = self.generate_progression(root, scale, numerals, octave)

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

            try:
                voicing = self._native.generate_chord_from_numeral(
                    numeral, root_pc, scale, 4
                )
                if voicing:
                    original_pcs = {n % 12 for n in voicing.notes}
                    neg_pcs = self._native.negative_harmony(original_pcs, root_pc)
                    neg_root = min(neg_pcs) if neg_pcs else root_pc
                else:
                    neg_root = root_pc
            except Exception:
                neg_root = root_pc

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
        return sorted(self._native.list_scale_names())

    def get_scale_info(self, scale_name: str) -> dict[str, Any] | None:
        """Get detailed information about a scale."""
        try:
            notes = self._native.generate_scale_notes(0, scale_name, 4)
            return {
                "name": scale_name,
                "intervals": [n - notes[0] for n in notes] if notes else [],
                "note_count": len(notes),
            }
        except Exception:
            return None


# Singleton instance
_engine: TheoryEngine | None = None


def get_engine() -> TheoryEngine:
    """Get the shared theory engine instance."""
    global _engine
    if _engine is None:
        _engine = TheoryEngine()
    return _engine
