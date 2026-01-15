"""Sunny - Theory Module.

Professional-grade music theory engine powered by music21 for:
- Scale-aware composition (all modes, exotic scales, symmetric scales)
- Chord progressions and harmonic analysis
- Functional harmony (negative harmony, secondary dominants)
- Voice leading with nearest-tone algorithm
- Cadence management
- Advanced rhythm (nested tuplets, metric modulation, polymeter)
"""

from sunny.theory.engine import TheoryEngine
from sunny.theory.scales import ScaleSystem
from sunny.theory.rhythm import RhythmEngine
from sunny.theory.harmony import HarmonyModule, FunctionalRole, ChordInfo
from sunny.theory.voiceleading import VoiceLeadingEngine
from sunny.theory.cadences import CadenceEngine, CadenceType

__all__ = [
    # Core engines
    "TheoryEngine",
    "ScaleSystem",
    "RhythmEngine",
    # Harmony
    "HarmonyModule",
    "FunctionalRole",
    "ChordInfo",
    # Voice leading
    "VoiceLeadingEngine",
    # Cadences
    "CadenceEngine",
    "CadenceType",
]

