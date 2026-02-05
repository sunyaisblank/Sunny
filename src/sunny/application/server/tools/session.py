"""Session Tools.

Component: SVTS001A
Domain: SV (Server) | Category: TS (Tools-Session)

Tools for session-level operations:
- Get session info (tempo, time signature, track count)
- Set tempo
- Transport controls (play, stop, jump)
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton, check_rate_limit
from sunny.infrastructure.security import ValidationError, validate_tempo

logger = logging.getLogger("sunny.tools.session")


@mcp.tool()
async def get_session_info(ctx: Context) -> str:
    """Get detailed information about the current Ableton session.

    Returns information including:
    - Tempo (BPM)
    - Time signature
    - Track count (MIDI, Audio, Return, Master)
    - Current playback state
    - Global scale/key (if set)

    Returns:
        JSON string containing session information
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_session_info")
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting session info: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_tempo(ctx: Context, bpm: float) -> str:
    """Set the project tempo in BPM.

    Args:
        bpm: Tempo in beats per minute (20.0 to 999.0)

    Returns:
        Confirmation message
    """
    try:
        check_rate_limit(ctx)
        validated_bpm = validate_tempo(bpm, "bpm")

        ableton = get_ableton(ctx)
        await ableton.send_command("set_tempo", {"bpm": validated_bpm})
        return f"Tempo set to {validated_bpm} BPM"
    except ValidationError as e:
        logger.warning(f"Validation error in set_tempo: {e}")
        return json.dumps({"error": str(e)})
    except Exception as e:
        logger.error(f"Error setting tempo: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def play(ctx: Context) -> str:
    """Start playback in Ableton.

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        await ableton.send_command("play")
        return "Playback started"
    except Exception as e:
        logger.error(f"Error starting playback: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def stop(ctx: Context) -> str:
    """Stop playback in Ableton.

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        await ableton.send_command("stop")
        return "Playback stopped"
    except Exception as e:
        logger.error(f"Error stopping playback: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def jump_to_bar(ctx: Context, bar: int, beat: float = 1.0) -> str:
    """Jump to a specific bar and beat position.

    Args:
        bar: Bar number (1-indexed)
        beat: Beat within the bar (1-indexed, default 1.0)

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        await ableton.send_command("jump_to_bar", {"bar": bar, "beat": beat})
        return f"Jumped to bar {bar}, beat {beat}"
    except Exception as e:
        logger.error(f"Error jumping to bar: {e}")
        return json.dumps({"error": str(e)})
