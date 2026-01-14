"""Sunny - Theory Module.

Professional-grade music theory engine powered by music21 for:
- Scale-aware composition (all modes, exotic scales, symmetric scales)
- Chord progressions and harmonic analysis
- Functional harmony (negative harmony, secondary dominants)
- Voice leading with nearest-tone algorithm
- Cadence management
- Advanced rhythm (nested tuplets, metric modulation, polymeter)
"""

from Sunny.Theory.engine import TheoryEngine
from Sunny.Theory.scales import ScaleSystem
from Sunny.Theory.rhythm import RhythmEngine
from Sunny.Theory.harmony import HarmonyModule, FunctionalRole, ChordInfo
from Sunny.Theory.voiceleading import VoiceLeadingEngine
from Sunny.Theory.cadences import CadenceEngine, CadenceType

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

