"""Harmony Tools.

Component: SVHY001A
Domain: SV (Server) | Category: HY (Tools-Harmony)

Advanced harmony and voice leading tools:
- Analyze progression functions
- Generate negative harmony progressions
- Add secondary dominants
- Get borrowed chords
- Voice leading optimization
- Cadence generation
- Melody generation
- Scale information
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_theory

logger = logging.getLogger("sunny.tools.harmony")


@mcp.tool()
async def analyze_progression_functions(
    ctx: Context,
    progression: list[str],
    mode: str = "major"
) -> str:
    """Analyze the functional harmony of a chord progression.

    Determines the harmonic function (Tonic/Subdominant/Dominant)
    and tension level of each chord.

    Args:
        progression: List of Roman numerals (e.g., ["ii", "V", "I"])
        mode: "major" or "minor"

    Returns:
        JSON array with function (T/S/D) and tension (0-2) for each chord
    """
    try:
        theory = get_theory(ctx)
        analysis = theory.analyze_progression_functions(progression, mode)
        return json.dumps(analysis, indent=2)
    except Exception as e:
        logger.error(f"Error analyzing progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_negative_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str]
) -> str:
    """Generate the negative harmony (mirror) version of a progression.

    Negative harmony mirrors chords around the tonic-dominant axis,
    transforming major to minor while maintaining harmonic function.
    Used by Jacob Collier and film composers for dark transformations.

    Args:
        root: Key root note (e.g., "C")
        scale: Scale/mode name
        numerals: Roman numerals to mirror (e.g., ["ii", "V", "I"])

    Returns:
        JSON array of mirrored chord info

    Example:
        ii-V-I in C major → bVII-iv-i (shadow version)
    """
    try:
        theory = get_theory(ctx)
        mirrored = theory.generate_negative_progression(root, scale, numerals)
        return json.dumps(mirrored, indent=2)
    except Exception as e:
        logger.error(f"Error generating negative progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def add_secondary_dominant(
    ctx: Context,
    progression: list[str],
    before_numeral: str
) -> str:
    """Insert a secondary dominant chord before a target chord.

    Secondary dominants (V/x) create chromatic tension approaching
    non-tonic chords. Essential for jazz and romantic harmony.

    Args:
        progression: Original progression numerals
        before_numeral: Chord to approach with secondary dominant

    Returns:
        Updated progression with V/x inserted

    Example:
        ["I", "ii", "V", "I"] with before_numeral="V"
        → ["I", "ii", "V/V", "V", "I"]
    """
    try:
        theory = get_theory(ctx)
        new_progression = theory.add_secondary_dominant(progression, before_numeral)
        return json.dumps({"progression": new_progression}, indent=2)
    except Exception as e:
        logger.error(f"Error adding secondary dominant: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_borrowed_chords(
    ctx: Context,
    key: str,
    mode: str = "major"
) -> str:
    """Get chords available through modal interchange.

    Modal interchange borrows chords from the parallel mode.
    In major, common borrowed chords include iv, bVI, bVII.

    Args:
        key: Key root (e.g., "C")
        mode: Current mode ("major" or "minor")

    Returns:
        JSON array of borrowable chords with numerals and source mode
    """
    try:
        theory = get_theory(ctx)
        borrowed = theory.get_borrowed_chords(key, mode)
        return json.dumps(borrowed, indent=2)
    except Exception as e:
        logger.error(f"Error getting borrowed chords: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_voiced_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str],
    octave: int = 4
) -> str:
    """Generate a progression with smooth voice leading.

    Uses the nearest-tone algorithm to minimize voice movement
    between chords, avoiding large leaps and creating professional
    part writing.

    Args:
        root: Key root note
        scale: Scale/mode name
        numerals: Roman numeral sequence
        octave: Base octave

    Returns:
        JSON array of chords with both block and voiced notes
    """
    try:
        theory = get_theory(ctx)
        progression = theory.generate_progression_voiced(root, scale, numerals, octave)
        return json.dumps(progression, indent=2)
    except Exception as e:
        logger.error(f"Error generating voiced progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_cadence(
    ctx: Context,
    cadence_type: str,
    key: str,
    mode: str = "major",
    octave: int = 4
) -> str:
    """Create a specific cadence type.

    Available types: perfect_authentic, imperfect_authentic, half,
    plagal, deceptive, picardy, neapolitan, phrygian, backdoor,
    minor_plagal

    Args:
        cadence_type: Type of cadence
        key: Key root note
        mode: "major" or "minor"
        octave: Base octave

    Returns:
        JSON array of chords forming the cadence
    """
    try:
        theory = get_theory(ctx)
        cadence = theory.create_cadence(cadence_type, key, mode, octave)
        return json.dumps(cadence, indent=2)
    except Exception as e:
        logger.error(f"Error creating cadence: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_cadence_types(ctx: Context) -> str:
    """List all available cadence types with descriptions.

    Returns:
        JSON array of cadence types with numerals and emotional qualities
    """
    try:
        theory = get_theory(ctx)
        types = theory.list_cadence_types()
        return json.dumps(types, indent=2)
    except Exception as e:
        logger.error(f"Error listing cadence types: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_melody(
    ctx: Context,
    root: str,
    scale: str,
    length: int = 8,
    octave: int = 5,
    contour: str = "arch"
) -> str:
    """Generate a scale-aware melody.

    Creates melodic lines that respect the given scale with
    natural-feeling contours and dynamics.

    Args:
        root: Scale root note (e.g., "C", "F#")
        scale: Scale/mode name (e.g., "major", "dorian", "phrygian_dominant")
        length: Number of notes to generate (default: 8)
        octave: Base octave (default: 5 for singable range)
        contour: Shape of melody ("arch", "ascending", "descending", "wave", "random")

    Returns:
        JSON array of note events with pitch, start_time, duration, velocity
    """
    try:
        theory = get_theory(ctx)
        melody = theory.generate_melody(root, scale, length, octave, None, contour)
        return json.dumps(melody, indent=2)
    except Exception as e:
        logger.error(f"Error generating melody: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_available_scales(ctx: Context) -> str:
    """List all available scales and modes.

    Returns over 40 scales including church modes, exotic scales,
    symmetric scales, and bebop scales.

    Returns:
        JSON array of scale names
    """
    try:
        theory = get_theory(ctx)
        scales = theory.get_available_scales()
        return json.dumps({
            "count": len(scales),
            "scales": sorted(scales)
        }, indent=2)
    except Exception as e:
        logger.error(f"Error listing scales: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_scale_info(ctx: Context, scale_name: str) -> str:
    """Get detailed information about a specific scale.

    Args:
        scale_name: Name of the scale (e.g., "phrygian_dominant", "whole_tone")

    Returns:
        JSON with intervals, chord qualities, and description
    """
    try:
        theory = get_theory(ctx)
        info = theory.get_scale_info(scale_name)
        if info is None:
            return json.dumps({"error": f"Scale '{scale_name}' not found"})
        return json.dumps(info, indent=2)
    except Exception as e:
        logger.error(f"Error getting scale info: {e}")
        return json.dumps({"error": str(e)})
