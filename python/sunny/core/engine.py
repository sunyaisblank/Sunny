"""Theory engine: a thin wrapper around the C++ backend.

Provides a Python-friendly interface to the C++ native backend.
All computation is delegated to the native extension; when an input
cannot be resolved (unknown root, unknown scale) the engine raises
rather than substituting a default.
"""

from __future__ import annotations

from typing import Any

from sunny.core import (
    NATIVE_AVAILABLE,
    NOTE_NAME_TO_PC,
    note_name,
)

# Roman numeral to degree mapping (used by Python-level helpers)
NUMERAL_TO_DEGREE = {
    "I": 0,
    "i": 0,
    "II": 1,
    "ii": 1,
    "III": 2,
    "iii": 2,
    "IV": 3,
    "iv": 3,
    "V": 4,
    "v": 4,
    "VI": 5,
    "vi": 5,
    "VII": 6,
    "vii": 6,
}

# Harmonic functions by degree (used by analyze_progression_functions)
HARMONIC_FUNCTIONS = {
    0: "T",  # I - Tonic
    1: "S",  # ii - Subdominant
    2: "T",  # iii - Tonic substitute
    3: "S",  # IV - Subdominant
    4: "D",  # V - Dominant
    5: "T",  # vi - Tonic substitute
    6: "D",  # vii(dim) - Dominant substitute
}


# Semitones above the root at which a tertian chord's fifth may lie, in order
# of preference: perfect, diminished, augmented. A chord containing both a
# perfect fifth and a sharp eleventh (6) or flat thirteenth (8) keeps 7.
_FIFTH_INTERVALS = (7, 6, 8)


def _chord_fifth(root: int, pitch_classes: set[int]) -> int:
    """Return the pitch class of a tertian chord's fifth.

    Raises:
        ValueError: If the chord has no perfect, diminished or augmented fifth.
    """
    for interval in _FIFTH_INTERVALS:
        candidate = (root + interval) % 12
        if candidate in pitch_classes:
            return candidate
    raise ValueError(f"Chord on pitch class {root} has no fifth to reflect")


def _parallel_minor_uses_flats(tonic: int) -> bool:
    """Whether the parallel minor of ``tonic`` has a flat key signature.

    Negative harmony maps a major key's chords into its parallel minor, so the
    reflected roots are spelled in that key: C gives B-flat, not A-sharp. The
    relative major of the parallel minor lies three semitones above the tonic;
    its signature has flats for F, B-flat, E-flat, A-flat and D-flat.
    """
    return (tonic + 3) % 12 in {5, 10, 3, 8, 1}


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
                "sunny_native C++ backend is required. Install a platform "
                "wheel or build the source checkout with 'uv sync'."
            )
        import sunny_native

        self._native = sunny_native

    def _root_pc(self, root: str) -> int:
        """Resolve a note name to a pitch class or decline."""
        pc = NOTE_NAME_TO_PC.get(root)
        if pc is None:
            raise ValueError(f"Unknown root note: {root!r}")
        return pc

    def _scale_intervals(self, scale: str) -> list[int]:
        """Resolve a scale name to semitone intervals or decline."""
        try:
            return list(self._native.scale_intervals(scale))
        except RuntimeError as e:
            raise ValueError(f"Unknown scale: {scale!r}") from e

    def get_scale_notes(self, root: str, mode: str, octave: int = 4) -> list[int]:
        """Get MIDI notes for a scale.

        Raises:
            ValueError: If the root or scale name is unknown.
        """
        root_pc = self._root_pc(root)
        intervals = self._scale_intervals(mode)
        return list(self._native.generate_scale_notes(root_pc, intervals, octave))

    def generate_progression(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4,
    ) -> list[dict[str, Any]]:
        """Generate a chord progression from Roman numerals.

        Raises:
            ValueError: If the root or scale is unknown.
            RuntimeError: If a numeral cannot be realised as a chord.
        """
        root_pc = self._root_pc(root)
        intervals = self._scale_intervals(scale)

        chords = []
        for numeral in numerals:
            voicing = self._native.generate_chord_from_numeral(numeral, root_pc, intervals, octave)
            notes = list(voicing.notes)
            chords.append(
                {
                    "numeral": numeral,
                    "root": note_name(voicing.root),
                    "quality": voicing.quality,
                    "notes": notes,
                }
            )

        return chords

    def generate_progression_voiced(
        self,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4,
    ) -> list[dict[str, Any]]:
        """Generate a progression with voice leading between chords."""
        chords = self.generate_progression(root, scale, numerals, octave)

        for i in range(1, len(chords)):
            source = chords[i - 1]["notes"]
            target_pcs = [n % 12 for n in chords[i]["notes"]]
            voiced = self.voice_lead(source, target_pcs, lock_bass=True)
            chords[i]["voiced_notes"] = voiced
            chords[i]["notes"] = voiced

        return chords

    def voice_lead(
        self,
        source: list[int],
        target_pcs: list[int],
        lock_bass: bool = False,
    ) -> list[int]:
        """Voice-lead source pitches to target pitch classes.

        Returns the voiced MIDI notes minimising total motion.
        """
        result = self._native.voice_lead_nearest_tone(source, target_pcs, lock_bass, False, False)
        return list(result.voiced_notes)

    def analyze_progression_functions(
        self,
        numerals: list[str],
        mode: str = "major",
    ) -> list[dict[str, Any]]:
        """Analyze harmonic functions of a simple diatonic progression.

        Raises:
            ValueError: If the mode or a numeral is not supported by this
                Python-level helper.
        """
        if mode not in {"major", "minor"}:
            raise ValueError(f"Unknown mode: {mode!r}")
        result = []
        for numeral in numerals:
            base = numeral.rstrip("°o+7")
            degree = NUMERAL_TO_DEGREE.get(base)
            if degree is None:
                raise ValueError(f"Unsupported Roman numeral: {numeral!r}")
            func = HARMONIC_FUNCTIONS[degree]

            tension = 0
            if func == "D":
                tension = 2
            elif func == "S":
                tension = 1

            result.append(
                {
                    "numeral": numeral,
                    "function": func,
                    "tension": tension,
                }
            )

        return result

    def generate_negative_progression(
        self,
        root: str,
        scale: str,
        numerals: list[str],
    ) -> list[dict[str, Any]]:
        """Generate the negative-harmony reflection of a progression.

        Raises:
            ValueError: If the root or scale is unknown.
            RuntimeError: If a numeral cannot be realised as a chord.
        """
        root_pc = self._root_pc(root)
        intervals = self._scale_intervals(scale)
        result = []

        # Reflection reverses the stack of thirds, so the image's root is the
        # image of the original chord's fifth (the upper boundary of its triad).
        prefer_flats = _parallel_minor_uses_flats(root_pc)
        for numeral in numerals:
            voicing = self._native.generate_chord_from_numeral(numeral, root_pc, intervals, 4)
            original_pcs = {n % 12 for n in voicing.notes}
            fifth = _chord_fifth(voicing.root, original_pcs)
            (neg_root,) = self._native.negative_harmony({fifth}, root_pc)

            result.append(
                {
                    "original": numeral,
                    "negative_root": note_name(neg_root, prefer_flats),
                }
            )

        return result

    def add_secondary_dominant(
        self,
        progression: list[str],
        before_numeral: str,
    ) -> list[str]:
        """Add secondary dominant before a target chord."""
        result = []
        for numeral in progression:
            if numeral == before_numeral:
                result.append(f"V/{before_numeral}")
            result.append(numeral)
        return result

    def get_available_scales(self) -> list[str]:
        """Get the sorted list of built-in scale names."""
        return sorted(self._native.list_scale_names())

    def get_scale_info(self, scale_name: str) -> dict[str, Any]:
        """Get intervals and note count for a scale.

        Raises:
            ValueError: If the scale name is unknown.
        """
        intervals = self._scale_intervals(scale_name)
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
