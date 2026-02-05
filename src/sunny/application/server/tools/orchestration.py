"""Orchestration Tools.

Component: SVOR001A
Domain: SV (Server) | Category: OR (Tools-Orchestration)

Tools for instrumentation guidance:
- Suggest instruments for emotional contexts
- Assign voices to chord tones
- List emotional colors
- List orchestral instruments

Note: OrchestrationGuide is planned for C++ migration (INOR001A).
Currently returns placeholder data.
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp

logger = logging.getLogger("sunny.tools.orchestration")


# =============================================================================
# Placeholder Data
# =============================================================================

# Instrument database (simplified)
_INSTRUMENTS = {
    "strings": [
        {"name": "Violin I", "register": "soprano", "range": [55, 103]},
        {"name": "Violin II", "register": "soprano", "range": [55, 103]},
        {"name": "Viola", "register": "alto", "range": [48, 91]},
        {"name": "Cello", "register": "tenor", "range": [36, 76]},
        {"name": "Contrabass", "register": "bass", "range": [28, 67]},
    ],
    "woodwinds": [
        {"name": "Flute", "register": "soprano", "range": [60, 96]},
        {"name": "Oboe", "register": "alto", "range": [58, 91]},
        {"name": "Clarinet", "register": "tenor", "range": [50, 94]},
        {"name": "Bassoon", "register": "bass", "range": [34, 75]},
    ],
    "brass": [
        {"name": "Trumpet", "register": "soprano", "range": [55, 82]},
        {"name": "French Horn", "register": "alto", "range": [34, 77]},
        {"name": "Trombone", "register": "tenor", "range": [40, 72]},
        {"name": "Tuba", "register": "bass", "range": [29, 58]},
    ],
    "synths": [
        {"name": "Lead Synth", "register": "soprano", "range": [48, 96]},
        {"name": "Pad Synth", "register": "alto", "range": [36, 84]},
        {"name": "Bass Synth", "register": "bass", "range": [24, 60]},
    ],
}

_EMOTIONAL_COLORS = {
    "heroic": ["Trumpet", "French Horn", "Strings"],
    "melancholy": ["Cello", "Oboe", "Viola"],
    "ethereal": ["Flute", "Violin I", "Pad Synth"],
    "climactic": ["Full Orchestra", "Timpani", "Brass"],
    "tension": ["Tremolo Strings", "Low Brass", "Percussion"],
    "pastoral": ["Flute", "Oboe", "French Horn"],
    "aggressive": ["Low Brass", "Percussion", "Bass Synth"],
    "mysterious": ["Low Strings", "Bassoon", "Muted Brass"],
    "romantic": ["Violin I", "Cello", "French Horn"],
    "playful": ["Flute", "Pizzicato Strings", "Staccato Woodwinds"],
}


# =============================================================================
# Tools
# =============================================================================


@mcp.tool()
async def suggest_instruments(
    ctx: Context,
    emotional_color: str,
    register: str | None = None,
    category: str | None = None,
    limit: int = 5
) -> str:
    """Suggest orchestral instruments for a given emotional context.

    Returns instruments that best convey the specified emotional quality,
    optionally filtered by voice register or instrument category.

    Args:
        emotional_color: Emotion to convey (heroic, melancholy, ethereal,
            climactic, tension, pastoral, aggressive, mysterious, romantic, playful)
        register: Optional voice register filter (bass, tenor, alto, soprano, super)
        category: Optional category filter (strings, woodwinds, brass, percussion,
            keyboards, synths)
        limit: Maximum number of suggestions (default: 5)

    Returns:
        JSON array of instrument suggestions with rationale
    """
    try:
        color_lower = emotional_color.lower()
        if color_lower not in _EMOTIONAL_COLORS:
            return json.dumps({
                "error": f"Unknown emotional color: {emotional_color}",
                "available": list(_EMOTIONAL_COLORS.keys()),
            })

        suggestions = []
        suggested_names = _EMOTIONAL_COLORS.get(color_lower, [])

        # Find matching instruments
        for cat, instruments in _INSTRUMENTS.items():
            if category and cat != category.lower():
                continue
            for inst in instruments:
                if register and inst["register"] != register.lower():
                    continue
                if inst["name"] in suggested_names or not suggested_names:
                    suggestions.append({
                        "instrument": inst["name"],
                        "category": cat,
                        "register": inst["register"],
                        "rationale": f"Suitable for {color_lower} emotional quality",
                    })
                if len(suggestions) >= limit:
                    break
            if len(suggestions) >= limit:
                break

        return json.dumps(suggestions[:limit], indent=2)
    except Exception as e:
        logger.error(f"Error suggesting instruments: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_voice_assignment(
    ctx: Context,
    chord_notes: list[int]
) -> str:
    """Assign chord tones to voice roles with instrument suggestions.

    Analyzes chord voicing and suggests appropriate instruments
    for each voice based on register and orchestral practice.

    Args:
        chord_notes: List of MIDI note numbers forming the chord

    Returns:
        JSON with voice assignments (bass, tenor, alto, soprano)
        and suggested instruments for each
    """
    try:
        if not chord_notes:
            return json.dumps({"error": "No chord notes provided"})

        sorted_notes = sorted(chord_notes)
        assignments = []

        # Map notes to voices based on position
        voice_names = ["bass", "tenor", "alto", "soprano"]
        for i, note in enumerate(sorted_notes):
            voice = voice_names[min(i, len(voice_names) - 1)]
            # Simple register classification
            if note < 48:
                suggested = "Contrabass or Bass Synth"
            elif note < 60:
                suggested = "Cello or Trombone"
            elif note < 72:
                suggested = "Viola or French Horn"
            else:
                suggested = "Violin or Flute"

            assignments.append({
                "note": note,
                "voice": voice,
                "suggested_instrument": suggested,
            })

        return json.dumps({"assignments": assignments}, indent=2)
    except Exception as e:
        logger.error(f"Error assigning voices: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_emotional_colors(ctx: Context) -> str:
    """List all available emotional colors for orchestration.

    Returns each emotional color with sample instruments that
    typically convey that emotion.

    Returns:
        JSON array of emotional colors with sample instruments
    """
    try:
        colors = [
            {"color": color, "sample_instruments": instruments}
            for color, instruments in _EMOTIONAL_COLORS.items()
        ]
        return json.dumps(colors, indent=2)
    except Exception as e:
        logger.error(f"Error listing colors: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_orchestral_instruments(
    ctx: Context,
    category: str | None = None
) -> str:
    """List available orchestral and electronic instruments.

    Provides a database of instruments with their registers,
    categories, and descriptions.

    Args:
        category: Optional filter (strings, woodwinds, brass, percussion,
            keyboards, synths, vocals)

    Returns:
        JSON array of instruments with details
    """
    try:
        result = []
        for cat, instruments in _INSTRUMENTS.items():
            if category and cat != category.lower():
                continue
            for inst in instruments:
                result.append({
                    "name": inst["name"],
                    "category": cat,
                    "register": inst["register"],
                    "range_low": inst["range"][0],
                    "range_high": inst["range"][1],
                })
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error listing instruments: {e}")
        return json.dumps({"error": str(e)})
