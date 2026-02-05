"""Theory Tools.

Component: SVTH001A
Domain: SV (Server) | Category: TH (Tools-Theory)

Tools for music theory computations:
- Generate chord progressions from Roman numerals
- Get scale notes
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_theory, check_rate_limit
from sunny.infrastructure.security import ValidationError, validate_note_name, validate_scale_name

logger = logging.getLogger("sunny.tools.theory")


@mcp.tool()
async def generate_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str],
    octave: int = 4
) -> str:
    """Generate a chord progression from Roman numerals.

    Uses music theory to create proper chord voicings based on
    the specified key and scale/mode.

    Args:
        root: Root note (e.g., "C", "F#", "Bb")
        scale: Scale/mode name (e.g., "major", "minor", "dorian", "phrygian")
        numerals: List of Roman numerals (e.g., ["ii", "V", "I"])
        octave: Base octave for chord voicings (default: 4)

    Returns:
        JSON array of chord objects, each containing:
        - numeral: The Roman numeral
        - root: Chord root note
        - quality: Chord quality (major, minor, diminished, etc.)
        - notes: MIDI note numbers

    Example:
        generate_progression("C", "major", ["ii", "V", "I"])
        Returns: Dm, G, C chords with MIDI notes
    """
    try:
        # Rate limit check
        check_rate_limit(ctx)

        # Input validation
        validated_root = validate_note_name(root, "root")
        validated_scale = validate_scale_name(scale, "scale")

        theory = get_theory(ctx)
        logger.info(f"Generating progression: {validated_root} {validated_scale} {numerals}")
        progression = theory.generate_progression(validated_root, validated_scale, numerals, octave)
        logger.info(f"Generated {len(progression)} chords")

        if not progression:
            logger.warning(f"Empty progression for {numerals} in {validated_root} {validated_scale}")

        return json.dumps(progression, indent=2)
    except ValidationError as e:
        logger.warning(f"Validation error in generate_progression: {e}")
        return json.dumps({"error": str(e)})
    except ValueError as e:
        logger.warning(f"Invalid theory input: {e}")
        return json.dumps({"error": f"Invalid theory input: {e}"})
    except Exception as e:
        logger.error(f"Error generating progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_scale_notes(
    ctx: Context,
    root: str,
    mode: str,
    octave: int = 4
) -> str:
    """Get MIDI note numbers for a scale.

    Args:
        root: Root note (e.g., "C", "F#")
        mode: Scale mode (ionian, dorian, phrygian, lydian,
              mixolydian, aeolian, locrian)
        octave: Octave for the scale (default: 4)

    Returns:
        JSON array of MIDI note numbers for scale degrees 1-7
    """
    try:
        theory = get_theory(ctx)
        notes = theory.get_scale_notes(root, mode, octave)
        return json.dumps({
            "root": root,
            "mode": mode,
            "octave": octave,
            "notes": notes,
        }, indent=2)
    except ValueError as e:
        return json.dumps({"error": f"Invalid scale: {e}"})
    except Exception as e:
        logger.error(f"Error getting scale notes: {e}")
        return json.dumps({"error": str(e)})
