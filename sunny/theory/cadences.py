"""Sunny - Cadence Management.

Component: THCA001A
Domain: TH (Theory) | Category: CA (Cadence)

Professional cadence generation and detection:
- Perfect, Imperfect, Half, Plagal, Deceptive cadences
- Neapolitan and Phrygian cadences
- Picardy third

Bounds:
    - key: str ∈ {valid_note_names}
    - mode: str ∈ {"major", "minor"}
    - octave: int ∈ [2, 6]
    - cadence_type: CadenceType enum

Error Codes:
    - 1320: Invalid cadence type
    - 1321: Invalid key
"""

from __future__ import annotations

import logging
from enum import Enum
from typing import Any, NamedTuple

logger = logging.getLogger("sunny.theory.cadences")


class CadenceType(Enum):
    """Types of musical cadences.
    
    Component: THCA001A.CadenceType
    """
    
    # Authentic cadences (V → I)
    PERFECT_AUTHENTIC = "perfect_authentic"      # V → I, root position, soprano on tonic
    IMPERFECT_AUTHENTIC = "imperfect_authentic"  # V → I, weaker (inversion or soprano not on 1)
    
    # Half cadence (ends on V)
    HALF = "half"                                # Any → V
    
    # Plagal cadence (IV → I)
    PLAGAL = "plagal"                            # IV → I, "Amen" cadence
    
    # Deceptive cadence (V → vi)
    DECEPTIVE = "deceptive"                      # V → vi, unexpected resolution
    
    # Specialized cadences
    PICARDY = "picardy"                          # Minor v → Major I (Picardy third)
    NEAPOLITAN = "neapolitan"                    # bII6 → V → I
    PHRYGIAN = "phrygian"                        # iv6 → V (Spanish/Baroque)
    
    # Less common
    BACKDOOR = "backdoor"                        # bVII → I (jazz)
    MINOR_PLAGAL = "minor_plagal"                # iv → I (borrowed from minor)


class CadenceInfo(NamedTuple):
    """Information about a cadence.
    
    Component: THCA001A.CadenceInfo
    """
    
    type: CadenceType
    numerals: tuple[str, ...]
    description: str
    emotional_quality: str


# Cadence definitions with emotional qualities
CADENCE_DEFINITIONS = {
    CadenceType.PERFECT_AUTHENTIC: CadenceInfo(
        type=CadenceType.PERFECT_AUTHENTIC,
        numerals=("V", "I"),
        description="Strongest resolution, complete finality",
        emotional_quality="triumphant, conclusive, satisfying",
    ),
    CadenceType.IMPERFECT_AUTHENTIC: CadenceInfo(
        type=CadenceType.IMPERFECT_AUTHENTIC,
        numerals=("V", "I6"),
        description="Weaker authentic, less final",
        emotional_quality="somewhat resolved, continuing",
    ),
    CadenceType.HALF: CadenceInfo(
        type=CadenceType.HALF,
        numerals=("I", "V"),
        description="Ends on dominant, incomplete feeling",
        emotional_quality="suspenseful, questioning, anticipation",
    ),
    CadenceType.PLAGAL: CadenceInfo(
        type=CadenceType.PLAGAL,
        numerals=("IV", "I"),
        description="Church/Amen cadence, soft resolution",
        emotional_quality="peaceful, devotional, gentle",
    ),
    CadenceType.DECEPTIVE: CadenceInfo(
        type=CadenceType.DECEPTIVE,
        numerals=("V", "vi"),
        description="Unexpected resolution to vi",
        emotional_quality="surprise, melancholy turn, introspective",
    ),
    CadenceType.PICARDY: CadenceInfo(
        type=CadenceType.PICARDY,
        numerals=("V", "I"),  # But I is major in a minor key
        description="Minor key ends on major tonic",
        emotional_quality="bright surprise, hope, redemption",
    ),
    CadenceType.NEAPOLITAN: CadenceInfo(
        type=CadenceType.NEAPOLITAN,
        numerals=("bII6", "V", "I"),
        description="Chromatic pre-dominant approach",
        emotional_quality="dramatic, operatic, dark nobility",
    ),
    CadenceType.PHRYGIAN: CadenceInfo(
        type=CadenceType.PHRYGIAN,
        numerals=("iv6", "V"),
        description="Half cadence with b2 in bass",
        emotional_quality="Spanish, exotic, baroque gravity",
    ),
    CadenceType.BACKDOOR: CadenceInfo(
        type=CadenceType.BACKDOOR,
        numerals=("bVII", "I"),
        description="Jazz backdoor resolution",
        emotional_quality="unexpected, hip, sophisticated",
    ),
    CadenceType.MINOR_PLAGAL: CadenceInfo(
        type=CadenceType.MINOR_PLAGAL,
        numerals=("iv", "I"),
        description="Minor plagal (borrowed iv)",
        emotional_quality="bittersweet, tender, nostalgic",
    ),
}


class CadenceEngine:
    """Engine for generating and detecting cadences.
    
    Component: THCA001A
    
    Cadences are the "punctuation" of music, providing varying
    degrees of resolution and emotional color at phrase endings.
    
    Bounds:
        - key: str ∈ {C, C#, Db, D, ..., B}
        - mode: str ∈ {"major", "minor"}
        - octave: int ∈ [2, 6]
        - cadence_type: CadenceType ∈ {all enum values}
    
    Error Codes:
        - 1320: Invalid cadence type
        - 1321: Invalid key
    """
    
    # Note to pitch class mapping
    NOTE_TO_PC = {
        "C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3,
        "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8,
        "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11,
    }
    
    NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    NOTE_NAMES_FLAT = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
    
    def get_cadence_info(self, cadence_type: CadenceType) -> CadenceInfo:
        """Get information about a cadence type.
        
        Args:
            cadence_type: The cadence type
        
        Returns:
            CadenceInfo with numerals and description
        """
        return CADENCE_DEFINITIONS.get(
            cadence_type,
            CADENCE_DEFINITIONS[CadenceType.PERFECT_AUTHENTIC]
        )
    
    def create_cadence(
        self,
        cadence_type: CadenceType | str,
        key: str,
        mode: str = "major",
        octave: int = 4
    ) -> list[dict[str, Any]]:
        """Generate a cadence in a given key.
        
        Args:
            cadence_type: Type of cadence (enum or string)
            key: Key root note
            mode: "major" or "minor"
            octave: Base octave for voicings
        
        Returns:
            List of chord dictionaries with notes
        
        Example:
            >>> engine.create_cadence(CadenceType.PERFECT_AUTHENTIC, "C", "major")
            [{"numeral": "V", "notes": [67, 71, 74]}, {"numeral": "I", "notes": [60, 64, 67]}]
        """
        # Handle string input
        if isinstance(cadence_type, str):
            cadence_type = CadenceType(cadence_type.lower())
        
        key_pc = self.NOTE_TO_PC.get(key, 0)
        
        # Get base MIDI for the octave
        base_midi = (octave + 1) * 12
        
        # Major scale intervals
        if mode.lower() == "major":
            scale = [0, 2, 4, 5, 7, 9, 11]
        else:
            scale = [0, 2, 3, 5, 7, 8, 10]  # Natural minor
        
        # Generate chords based on cadence type
        if cadence_type == CadenceType.PERFECT_AUTHENTIC:
            return self._create_authentic_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.IMPERFECT_AUTHENTIC:
            return self._create_imperfect_authentic(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.HALF:
            return self._create_half_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.PLAGAL:
            return self._create_plagal_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.DECEPTIVE:
            return self._create_deceptive_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.PICARDY:
            return self._create_picardy_cadence(key_pc, base_midi, scale)
        
        elif cadence_type == CadenceType.NEAPOLITAN:
            return self._create_neapolitan_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.PHRYGIAN:
            return self._create_phrygian_cadence(key_pc, base_midi, scale)
        
        elif cadence_type == CadenceType.BACKDOOR:
            return self._create_backdoor_cadence(key_pc, base_midi, scale, mode)
        
        elif cadence_type == CadenceType.MINOR_PLAGAL:
            return self._create_minor_plagal(key_pc, base_midi, scale)
        
        else:
            # Default to perfect authentic
            return self._create_authentic_cadence(key_pc, base_midi, scale, mode)
    
    def _build_chord(
        self,
        root_pc: int,
        quality: str,
        base_midi: int
    ) -> tuple[int, ...]:
        """Build a chord from root and quality."""
        root_midi = base_midi + root_pc
        
        intervals = {
            "major": (0, 4, 7),
            "minor": (0, 3, 7),
            "diminished": (0, 3, 6),
            "augmented": (0, 4, 8),
            "dominant7": (0, 4, 7, 10),
            "major7": (0, 4, 7, 11),
            "minor7": (0, 3, 7, 10),
        }
        
        return tuple(root_midi + i for i in intervals.get(quality, (0, 4, 7)))
    
    def _create_authentic_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """V → I perfect authentic cadence."""
        # V chord (dominant)
        v_root = (key_pc + scale[4]) % 12  # 5th degree
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        # I chord (tonic)
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_chord = self._build_chord(i_root, i_quality, base_midi)
        
        return [
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": list(i_chord)},
        ]
    
    def _create_imperfect_authentic(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """V → I6 imperfect authentic cadence (first inversion tonic)."""
        # V chord
        v_root = (key_pc + scale[4]) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        # I6 chord (first inversion - 3rd in bass)
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_notes = list(self._build_chord(i_root, i_quality, base_midi))
        # First inversion: move root up an octave
        i_notes[0] += 12
        i_notes.sort()
        
        return [
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
            {"numeral": "I6", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": i_notes},
        ]
    
    def _create_half_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """I → V half cadence (ends on dominant)."""
        # I chord
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_chord = self._build_chord(i_root, i_quality, base_midi)
        
        # V chord
        v_root = (key_pc + scale[4]) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        return [
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": list(i_chord)},
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
        ]
    
    def _create_plagal_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """IV → I plagal (Amen) cadence."""
        # IV chord
        iv_root = (key_pc + scale[3]) % 12
        iv_quality = "major" if mode == "major" else "minor"
        iv_chord = self._build_chord(iv_root, iv_quality, base_midi)
        
        # I chord
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_chord = self._build_chord(i_root, i_quality, base_midi)
        
        return [
            {"numeral": "IV", "root": self.NOTE_NAMES[iv_root], "quality": iv_quality, "notes": list(iv_chord)},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": list(i_chord)},
        ]
    
    def _create_deceptive_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """V → vi deceptive cadence."""
        # V chord
        v_root = (key_pc + scale[4]) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        # vi chord
        vi_root = (key_pc + scale[5]) % 12
        vi_quality = "minor" if mode == "major" else "major"  # Opposite of tonic
        vi_chord = self._build_chord(vi_root, vi_quality, base_midi)
        
        return [
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
            {"numeral": "vi", "root": self.NOTE_NAMES[vi_root], "quality": vi_quality, "notes": list(vi_chord)},
        ]
    
    def _create_picardy_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int]
    ) -> list[dict[str, Any]]:
        """V → I (major) Picardy third in minor key."""
        # Use minor scale for context but major I
        minor_scale = [0, 2, 3, 5, 7, 8, 10]
        
        # V chord (from harmonic minor - major V)
        v_root = (key_pc + 7) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        # I MAJOR chord (the Picardy third)
        i_root = key_pc
        i_chord = self._build_chord(i_root, "major", base_midi)
        
        return [
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": "major", "notes": list(i_chord), 
             "note": "Picardy third - major tonic in minor key"},
        ]
    
    def _create_neapolitan_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """bII6 → V → I Neapolitan cadence."""
        # bII chord (Neapolitan) - major chord on b2
        bii_root = (key_pc + 1) % 12
        bii_notes = list(self._build_chord(bii_root, "major", base_midi))
        # First inversion
        bii_notes[0] += 12
        bii_notes.sort()
        
        # V chord
        v_root = (key_pc + scale[4]) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        # I chord
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_chord = self._build_chord(i_root, i_quality, base_midi)
        
        return [
            {"numeral": "bII6", "root": self.NOTE_NAMES_FLAT[bii_root], "quality": "major", "notes": bii_notes},
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": list(i_chord)},
        ]
    
    def _create_phrygian_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int]
    ) -> list[dict[str, Any]]:
        """iv6 → V Phrygian half cadence."""
        # iv chord in first inversion (b6 in bass)
        iv_root = (key_pc + 5) % 12
        iv_notes = list(self._build_chord(iv_root, "minor", base_midi))
        # First inversion
        iv_notes[0] += 12
        iv_notes.sort()
        
        # V chord
        v_root = (key_pc + 7) % 12
        v_chord = self._build_chord(v_root, "major", base_midi)
        
        return [
            {"numeral": "iv6", "root": self.NOTE_NAMES[iv_root], "quality": "minor", "notes": iv_notes},
            {"numeral": "V", "root": self.NOTE_NAMES[v_root], "quality": "major", "notes": list(v_chord)},
        ]
    
    def _create_backdoor_cadence(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int],
        mode: str
    ) -> list[dict[str, Any]]:
        """bVII → I backdoor cadence (jazz)."""
        # bVII chord
        bvii_root = (key_pc + 10) % 12
        bvii_chord = self._build_chord(bvii_root, "major", base_midi)
        
        # I chord
        i_root = key_pc
        i_quality = "major" if mode == "major" else "minor"
        i_chord = self._build_chord(i_root, i_quality, base_midi)
        
        return [
            {"numeral": "bVII", "root": self.NOTE_NAMES_FLAT[bvii_root], "quality": "major", "notes": list(bvii_chord)},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": i_quality, "notes": list(i_chord)},
        ]
    
    def _create_minor_plagal(
        self,
        key_pc: int,
        base_midi: int,
        scale: list[int]
    ) -> list[dict[str, Any]]:
        """iv → I minor plagal (borrowed iv in major)."""
        # iv chord (minor - borrowed from parallel minor)
        iv_root = (key_pc + 5) % 12
        iv_chord = self._build_chord(iv_root, "minor", base_midi)
        
        # I chord (major tonic)
        i_root = key_pc
        i_chord = self._build_chord(i_root, "major", base_midi)
        
        return [
            {"numeral": "iv", "root": self.NOTE_NAMES[iv_root], "quality": "minor", "notes": list(iv_chord),
             "note": "Borrowed from parallel minor"},
            {"numeral": "I", "root": self.NOTE_NAMES[i_root], "quality": "major", "notes": list(i_chord)},
        ]
    
    # ==========================================================================
    # Cadence Detection
    # ==========================================================================
    
    def detect_cadence(
        self,
        numerals: list[str]
    ) -> CadenceType | None:
        """Detect the cadence type from a sequence of Roman numerals.
        
        Args:
            numerals: Last 2-3 chords of a phrase
        
        Returns:
            Detected cadence type or None
        
        Example:
            >>> engine.detect_cadence(["V", "I"])
            CadenceType.PERFECT_AUTHENTIC
        """
        if len(numerals) < 2:
            return None
        
        last_two = [n.upper().rstrip("7654") for n in numerals[-2:]]
        
        # Check common patterns
        if last_two == ["V", "I"]:
            return CadenceType.PERFECT_AUTHENTIC
        elif last_two == ["IV", "I"]:
            return CadenceType.PLAGAL
        elif last_two == ["V", "VI"]:
            return CadenceType.DECEPTIVE
        elif last_two[1] == "V":
            return CadenceType.HALF
        
        # Check for Neapolitan (3-chord pattern)
        if len(numerals) >= 3:
            last_three = [n.upper().rstrip("7654") for n in numerals[-3:]]
            if "BII" in last_three[0] and last_three[1] == "V" and last_three[2] == "I":
                return CadenceType.NEAPOLITAN
        
        return None
    
    def extend_with_cadence(
        self,
        progression: list[str],
        cadence_type: CadenceType
    ) -> list[str]:
        """Extend a progression with a specific cadence.
        
        Args:
            progression: Existing progression numerals
            cadence_type: Type of cadence to add
        
        Returns:
            Extended progression
        """
        info = self.get_cadence_info(cadence_type)
        
        # Remove last chord if it overlaps with cadence start
        result = list(progression)
        
        # Append cadence numerals
        for numeral in info.numerals:
            result.append(numeral)
        
        return result
    
    def list_cadence_types(self) -> list[dict[str, str]]:
        """List all available cadence types with descriptions.
        
        Returns:
            List of cadence type information
        """
        return [
            {
                "type": cad_type.value,
                "numerals": " → ".join(info.numerals),
                "description": info.description,
                "emotional_quality": info.emotional_quality,
            }
            for cad_type, info in CADENCE_DEFINITIONS.items()
        ]
