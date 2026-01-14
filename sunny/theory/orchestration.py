"""Sunny - Orchestration Guidance.

Component: THOR001A
Domain: TH (Theory) | Category: OR (Orchestration)

Provides instrument suggestions and timbral guidance based on
musical context, register, and emotional intent.

Bounds:
    - emotional_color: EmotionalColor ∈ {all enum values}
    - register: InstrumentRole ∈ {bass, tenor, alto, soprano, super}
    - limit: int ∈ [1, 20]

Error Codes:
    - 1340: Invalid emotional color
    - 1341: Invalid register
"""

from __future__ import annotations

from enum import Enum
from typing import NamedTuple


class InstrumentRole(Enum):
    """Voice role based on frequency register.
    
    Component: THOR001A.InstrumentRole
    """
    BASS = "bass"           # Lowest voices (tubas, double bass, bass synths)
    TENOR = "tenor"         # Lower-mid voices (cello, trombone, baritone)
    ALTO = "alto"           # Mid voices (viola, french horn, clarinet)
    SOPRANO = "soprano"     # High voices (violin, flute, oboe)
    SUPER = "super"         # Very high (piccolo, celesta, glockenspiel)


class EmotionalColor(Enum):
    """Emotional quality to convey.
    
    Component: THOR001A.EmotionalColor
    """
    HEROIC = "heroic"               # Bold, triumphant, brass-forward
    MELANCHOLY = "melancholy"       # Sad, introspective, strings
    TENSION = "tension"             # Suspense, anxiety, dissonance
    ETHEREAL = "ethereal"           # Dreamy, heavenly, high strings
    CLIMACTIC = "climactic"         # Epic, full orchestra
    PASTORAL = "pastoral"           # Peaceful, nature, woodwinds
    AGGRESSIVE = "aggressive"       # Intense, driving, percussion
    MYSTERIOUS = "mysterious"       # Unknown, questioning, muted
    ROMANTIC = "romantic"           # Warm, expressive, strings
    PLAYFUL = "playful"             # Light, bouncy, staccato


class InstrumentSuggestion(NamedTuple):
    """Suggested instrument with rationale.
    
    Component: THOR001A.InstrumentSuggestion
    """
    name: str
    category: str
    register: InstrumentRole
    description: str


# Instrument database organized by category
ORCHESTRAL_INSTRUMENTS = {
    # Strings
    "violin": InstrumentSuggestion(
        "Violin", "strings", InstrumentRole.SOPRANO,
        "Primary melody, expressive vibrato, wide dynamic range"
    ),
    "viola": InstrumentSuggestion(
        "Viola", "strings", InstrumentRole.ALTO,
        "Warm inner voice, darker than violin"
    ),
    "cello": InstrumentSuggestion(
        "Cello", "strings", InstrumentRole.TENOR,
        "Rich tenor melodies, emotional depth, versatile"
    ),
    "double_bass": InstrumentSuggestion(
        "Double Bass", "strings", InstrumentRole.BASS,
        "Foundation, gravitas, pizzicato for rhythm"
    ),
    "harp": InstrumentSuggestion(
        "Harp", "strings", InstrumentRole.SOPRANO,
        "Ethereal arpeggios, glissandos, color"
    ),
    
    # Woodwinds
    "flute": InstrumentSuggestion(
        "Flute", "woodwinds", InstrumentRole.SOPRANO,
        "Bright, agile, pastoral melodies"
    ),
    "piccolo": InstrumentSuggestion(
        "Piccolo", "woodwinds", InstrumentRole.SUPER,
        "Piercing brightness, military, excitement"
    ),
    "oboe": InstrumentSuggestion(
        "Oboe", "woodwinds", InstrumentRole.SOPRANO,
        "Plaintive, expressive, pastoral"
    ),
    "clarinet": InstrumentSuggestion(
        "Clarinet", "woodwinds", InstrumentRole.ALTO,
        "Smooth, lyrical, wide range"
    ),
    "bass_clarinet": InstrumentSuggestion(
        "Bass Clarinet", "woodwinds", InstrumentRole.TENOR,
        "Dark, mysterious, low woodwind color"
    ),
    "bassoon": InstrumentSuggestion(
        "Bassoon", "woodwinds", InstrumentRole.TENOR,
        "Characterful, comedic or dramatic, versatile"
    ),
    "contrabassoon": InstrumentSuggestion(
        "Contrabassoon", "woodwinds", InstrumentRole.BASS,
        "Deep rumble, reinforces bass, ominous"
    ),
    
    # Brass
    "french_horn": InstrumentSuggestion(
        "French Horn", "brass", InstrumentRole.ALTO,
        "Noble, heroic, blends with strings and woodwinds"
    ),
    "trumpet": InstrumentSuggestion(
        "Trumpet", "brass", InstrumentRole.SOPRANO,
        "Brilliant, heroic fanfares, bold statements"
    ),
    "trombone": InstrumentSuggestion(
        "Trombone", "brass", InstrumentRole.TENOR,
        "Power, majesty, smooth glissandos"
    ),
    "bass_trombone": InstrumentSuggestion(
        "Bass Trombone", "brass", InstrumentRole.BASS,
        "Deep brass foundation, power"
    ),
    "tuba": InstrumentSuggestion(
        "Tuba", "brass", InstrumentRole.BASS,
        "Deep brass foundation, weight"
    ),
    
    # Percussion
    "timpani": InstrumentSuggestion(
        "Timpani", "percussion", InstrumentRole.BASS,
        "Dramatic rolls, punctuation, tension"
    ),
    "snare_drum": InstrumentSuggestion(
        "Snare Drum", "percussion", InstrumentRole.ALTO,
        "Military, rhythm, tension builds"
    ),
    "bass_drum": InstrumentSuggestion(
        "Bass Drum", "percussion", InstrumentRole.BASS,
        "Impact, climactic moments, weight"
    ),
    "cymbals": InstrumentSuggestion(
        "Cymbals", "percussion", InstrumentRole.SOPRANO,
        "Crashes for climax, suspense with rolls"
    ),
    "glockenspiel": InstrumentSuggestion(
        "Glockenspiel", "percussion", InstrumentRole.SUPER,
        "Sparkling, magical, high accents"
    ),
    "celesta": InstrumentSuggestion(
        "Celesta", "percussion", InstrumentRole.SUPER,
        "Ethereal, magical, delicate"
    ),
    "marimba": InstrumentSuggestion(
        "Marimba", "percussion", InstrumentRole.ALTO,
        "Warm, mellow, ostinatos"
    ),
    "vibraphone": InstrumentSuggestion(
        "Vibraphone", "percussion", InstrumentRole.SOPRANO,
        "Jazz, shimmer, sustained notes"
    ),
    
    # Keyboards
    "piano": InstrumentSuggestion(
        "Piano", "keyboards", InstrumentRole.SOPRANO,
        "Versatile, full range, solo or accompaniment"
    ),
    "organ": InstrumentSuggestion(
        "Organ", "keyboards", InstrumentRole.TENOR,
        "Sustained power, ecclesiastical, full"
    ),
}

# Electronic/modern instruments for Ableton context
ELECTRONIC_INSTRUMENTS = {
    "bass_synth": InstrumentSuggestion(
        "Bass Synthesizer", "synths", InstrumentRole.BASS,
        "Deep sub-bass, impact, modern power"
    ),
    "pad_synth": InstrumentSuggestion(
        "Pad Synthesizer", "synths", InstrumentRole.ALTO,
        "Atmospheric, ambient, sustained textures"
    ),
    "lead_synth": InstrumentSuggestion(
        "Lead Synthesizer", "synths", InstrumentRole.SOPRANO,
        "Cutting melodies, aggressive or smooth"
    ),
    "arp_synth": InstrumentSuggestion(
        "Arpeggiator Synth", "synths", InstrumentRole.SOPRANO,
        "Rhythmic patterns, motion, energy"
    ),
    "pluck_synth": InstrumentSuggestion(
        "Pluck Synthesizer", "synths", InstrumentRole.SOPRANO,
        "Clean percussive attacks, arpeggios"
    ),
    "choir": InstrumentSuggestion(
        "Choir/Vocal Pad", "vocals", InstrumentRole.SOPRANO,
        "Ethereal, human, emotional"
    ),
}

# Emotional color to instrument mappings
EMOTION_INSTRUMENT_MAP: dict[EmotionalColor, list[str]] = {
    EmotionalColor.HEROIC: [
        "french_horn", "trumpet", "trombone", "timpani", "violin", "snare_drum"
    ],
    EmotionalColor.MELANCHOLY: [
        "cello", "viola", "oboe", "english_horn", "bass_clarinet", "piano"
    ],
    EmotionalColor.TENSION: [
        "timpani", "bass_drum", "bass_trombone", "contrabassoon", "snare_drum"
    ],
    EmotionalColor.ETHEREAL: [
        "celesta", "glockenspiel", "harp", "flute", "violin", "choir", "pad_synth"
    ],
    EmotionalColor.CLIMACTIC: [
        "timpani", "cymbals", "trumpet", "trombone", "tuba", "bass_drum", "violin"
    ],
    EmotionalColor.PASTORAL: [
        "flute", "oboe", "clarinet", "french_horn", "harp", "violin"
    ],
    EmotionalColor.AGGRESSIVE: [
        "bass_drum", "snare_drum", "trombone", "trumpet", "timpani", "bass_synth"
    ],
    EmotionalColor.MYSTERIOUS: [
        "bass_clarinet", "bassoon", "harp", "celesta", "timpani", "pad_synth"
    ],
    EmotionalColor.ROMANTIC: [
        "violin", "cello", "french_horn", "clarinet", "harp", "piano"
    ],
    EmotionalColor.PLAYFUL: [
        "piccolo", "flute", "clarinet", "glockenspiel", "pizzicato_strings", "pluck_synth"
    ],
}


class OrchestrationGuide:
    """Provides instrument suggestions based on musical context."""
    
    def __init__(self):
        """Initialize the orchestration guide."""
        self._instruments = {**ORCHESTRAL_INSTRUMENTS, **ELECTRONIC_INSTRUMENTS}
    
    def suggest_instruments(
        self,
        emotional_color: str | EmotionalColor,
        register: str | InstrumentRole | None = None,
        category: str | None = None,
        limit: int = 5
    ) -> list[dict]:
        """Suggest instruments for a given emotional context.
        
        Args:
            emotional_color: Emotional quality to convey
            register: Optional voice register filter
            category: Optional instrument category filter
            limit: Maximum number of suggestions
        
        Returns:
            List of instrument suggestions with rationale
        """
        # Normalize emotional color
        if isinstance(emotional_color, str):
            try:
                emotional_color = EmotionalColor(emotional_color.lower())
            except ValueError:
                emotional_color = EmotionalColor.ROMANTIC  # Default
        
        # Normalize register
        if isinstance(register, str):
            try:
                register = InstrumentRole(register.lower())
            except ValueError:
                register = None
        
        # Get instruments for this emotion
        instrument_names = EMOTION_INSTRUMENT_MAP.get(emotional_color, [])
        
        suggestions = []
        for name in instrument_names:
            if name not in self._instruments:
                continue
            
            inst = self._instruments[name]
            
            # Filter by register
            if register and inst.register != register:
                continue
            
            # Filter by category
            if category and inst.category != category.lower():
                continue
            
            suggestions.append({
                "name": inst.name,
                "category": inst.category,
                "register": inst.register.value,
                "description": inst.description,
                "emotional_fit": emotional_color.value,
            })
            
            if len(suggestions) >= limit:
                break
        
        return suggestions
    
    def get_voice_assignment(
        self,
        chord_notes: list[int]
    ) -> dict[str, list[dict]]:
        """Assign chord tones to frequency roles.
        
        Args:
            chord_notes: List of MIDI note numbers
        
        Returns:
            Dict mapping voice role to notes with suggested instruments
        """
        sorted_notes = sorted(chord_notes)
        
        assignments: dict[str, list[dict]] = {
            "bass": [],
            "tenor": [],
            "alto": [],
            "soprano": [],
        }
        
        for i, note in enumerate(sorted_notes):
            # Determine role by position and octave
            octave = note // 12
            
            if i == 0:  # Lowest note
                role = "bass"
            elif octave <= 3:
                role = "tenor"
            elif octave <= 4:
                role = "alto"
            else:
                role = "soprano"
            
            # Get suggested instruments for this role
            role_enum = InstrumentRole(role)
            instruments = [
                name for name, inst in self._instruments.items()
                if inst.register == role_enum
            ][:3]  # Top 3 options
            
            assignments[role].append({
                "note": note,
                "octave": octave,
                "suggested_instruments": instruments,
            })
        
        return assignments
    
    def get_ableton_device_suggestions(
        self,
        instrument: str
    ) -> list[str]:
        """Map orchestral instrument to Ableton device URIs.
        
        Args:
            instrument: Instrument name
        
        Returns:
            List of Ableton browser URIs for matching devices
        """
        # Common mappings from orchestral to Ableton
        device_map = {
            "violin": ["Instruments/Orchestra/Strings/Violin"],
            "cello": ["Instruments/Orchestra/Strings/Cello"],
            "viola": ["Instruments/Orchestra/Strings/Viola"],
            "double_bass": ["Instruments/Orchestra/Strings/Bass"],
            "flute": ["Instruments/Orchestra/Woodwinds/Flute"],
            "oboe": ["Instruments/Orchestra/Woodwinds/Oboe"],
            "clarinet": ["Instruments/Orchestra/Woodwinds/Clarinet"],
            "bassoon": ["Instruments/Orchestra/Woodwinds/Bassoon"],
            "french_horn": ["Instruments/Orchestra/Brass/French Horn"],
            "trumpet": ["Instruments/Orchestra/Brass/Trumpet"],
            "trombone": ["Instruments/Orchestra/Brass/Trombone"],
            "tuba": ["Instruments/Orchestra/Brass/Tuba"],
            "timpani": ["Instruments/Drums/Orchestral/Timpani"],
            "piano": ["Instruments/Piano & Keys/Grand Piano"],
            "harp": ["Instruments/Orchestra/Harp"],
            "celesta": ["Instruments/Piano & Keys/Celesta"],
            "bass_synth": ["Instruments/Synthesizer/Bass"],
            "pad_synth": ["Instruments/Synthesizer/Pad"],
            "lead_synth": ["Instruments/Synthesizer/Lead"],
        }
        
        return device_map.get(instrument.lower(), [])
    
    def list_emotional_colors(self) -> list[dict]:
        """List all available emotional colors.
        
        Returns:
            List of emotional colors with sample instruments
        """
        return [
            {
                "color": color.value,
                "sample_instruments": EMOTION_INSTRUMENT_MAP.get(color, [])[:3],
            }
            for color in EmotionalColor
        ]
    
    def list_instruments(
        self,
        category: str | None = None
    ) -> list[dict]:
        """List available instruments.
        
        Args:
            category: Optional category filter
        
        Returns:
            List of instrument info
        """
        instruments = []
        for name, inst in self._instruments.items():
            if category and inst.category != category.lower():
                continue
            
            instruments.append({
                "id": name,
                "name": inst.name,
                "category": inst.category,
                "register": inst.register.value,
                "description": inst.description,
            })
        
        return instruments
