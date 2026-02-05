"""Mixer Tools.

Component: SVTM001A
Domain: SV (Server) | Category: TM (Tools-Mixer)

Tools for volume, pan, mute, and solo controls:
- Set track volume
- Set track pan
- Mute/unmute tracks
- Solo/unsolo tracks
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.mixer")


@mcp.tool()
async def set_track_volume(
    ctx: Context,
    track_index: int,
    volume_db: float
) -> str:
    """Set track volume in dB.

    Args:
        track_index: Index of the track
        volume_db: Volume in dB (-inf to +6)

    Returns:
        Confirmation with new volume
    """
    try:
        ableton = get_ableton(ctx)

        # Convert dB to normalized 0-1 value
        # Ableton's volume range is -inf to +6dB
        # Approximate: 0dB ≈ 0.85, -6dB ≈ 0.7, +6dB = 1.0
        if volume_db <= -70:
            normalized = 0.0
        else:
            # Linear approximation
            normalized = max(0.0, min(1.0, (volume_db + 70) / 76))

        result = await ableton.send_command("set_track_volume", {
            "track_index": track_index,
            "volume": normalized,
        })
        return f"Track {track_index} volume set to {volume_db} dB"
    except Exception as e:
        logger.error(f"Error setting track volume: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_track_pan(
    ctx: Context,
    track_index: int,
    pan: float
) -> str:
    """Set track panning.

    Args:
        track_index: Index of the track
        pan: Pan position (-1.0 = left, 0.0 = center, 1.0 = right)

    Returns:
        Confirmation with new pan position
    """
    try:
        ableton = get_ableton(ctx)

        pan = max(-1.0, min(1.0, pan))

        result = await ableton.send_command("set_track_pan", {
            "track_index": track_index,
            "pan": pan,
        })

        pan_str = "center" if abs(pan) < 0.05 else f"{'left' if pan < 0 else 'right'} {abs(int(pan * 50))}"
        return f"Track {track_index} pan set to {pan_str}"
    except Exception as e:
        logger.error(f"Error setting track pan: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def mute_track(ctx: Context, track_index: int, mute: bool = True) -> str:
    """Mute or unmute a track.

    Args:
        track_index: Index of the track
        mute: True to mute, False to unmute (default: True)

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_track_mute", {
            "track_index": track_index,
            "mute": mute,
        })
        return f"Track {track_index} {'muted' if mute else 'unmuted'}"
    except Exception as e:
        logger.error(f"Error muting track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def solo_track(ctx: Context, track_index: int, solo: bool = True) -> str:
    """Solo or unsolo a track.

    Args:
        track_index: Index of the track
        solo: True to solo, False to unsolo (default: True)

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_track_solo", {
            "track_index": track_index,
            "solo": solo,
        })
        return f"Track {track_index} {'soloed' if solo else 'unsoloed'}"
    except Exception as e:
        logger.error(f"Error soloing track: {e}")
        return json.dumps({"error": str(e)})
