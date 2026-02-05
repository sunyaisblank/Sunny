"""Arrangement Tools.

Component: SVTA001A
Domain: SV (Server) | Category: TA (Tools-Arrangement)

Tools for timeline and locator operations:
- Place clips in arrangement
- Create and navigate locators
- Set loop regions
- Duplicate arrangement sections
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.arrangement")


@mcp.tool()
async def place_clip_in_arrangement(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    bar: int,
    beat: float = 1.0
) -> str:
    """Place a clip from session view into the arrangement view at a specific position.

    Args:
        track_index: Index of the track containing the clip
        clip_slot: Clip slot index in session view
        bar: Bar number to place the clip (1-indexed)
        beat: Beat within the bar (default: 1.0 = first beat)

    Returns:
        Confirmation with placement position
    """
    try:
        ableton = get_ableton(ctx)

        # Convert bar/beat to beats from start
        # Assuming 4/4 time signature for now
        position_beats = (bar - 1) * 4 + (beat - 1)

        result = await ableton.send_command("place_clip_in_arrangement", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "position": position_beats,
        })
        return json.dumps({
            "status": "success",
            "message": f"Placed clip at bar {bar}, beat {beat}",
            "position_beats": position_beats,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error placing clip in arrangement: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_locator(
    ctx: Context,
    name: str,
    bar: int,
    beat: float = 1.0
) -> str:
    """Create a locator (marker) at a specific position in the arrangement.

    Args:
        name: Name for the locator (e.g., "Verse 1", "Chorus", "Bridge")
        bar: Bar number for the locator (1-indexed)
        beat: Beat within the bar (default: 1.0)

    Returns:
        JSON with locator info
    """
    try:
        ableton = get_ableton(ctx)

        position_beats = (bar - 1) * 4 + (beat - 1)

        result = await ableton.send_command("create_locator", {
            "name": name,
            "position": position_beats,
        })
        return json.dumps({
            "status": "success",
            "name": name,
            "bar": bar,
            "beat": beat,
            "position_beats": position_beats,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error creating locator: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_locators(ctx: Context) -> str:
    """Get all locators in the arrangement.

    Returns:
        JSON array of locators with names and positions
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_locators")
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting locators: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def jump_to_locator(ctx: Context, name: str) -> str:
    """Jump to a named locator in the arrangement.

    Args:
        name: Name of the locator to jump to

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("jump_to_locator", {"name": name})
        return f"Jumped to locator: {name}"
    except Exception as e:
        logger.error(f"Error jumping to locator: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_loop_region(
    ctx: Context,
    start_bar: int,
    end_bar: int,
    start_beat: float = 1.0,
    end_beat: float = 1.0
) -> str:
    """Set the loop region in the arrangement.

    Args:
        start_bar: Starting bar number (1-indexed)
        end_bar: Ending bar number (1-indexed)
        start_beat: Starting beat within bar (default: 1.0)
        end_beat: Ending beat within bar (default: 1.0)

    Returns:
        Confirmation with loop region details
    """
    try:
        ableton = get_ableton(ctx)

        start_beats = (start_bar - 1) * 4 + (start_beat - 1)
        end_beats = (end_bar - 1) * 4 + (end_beat - 1)

        result = await ableton.send_command("set_loop_region", {
            "start": start_beats,
            "end": end_beats,
        })
        return json.dumps({
            "status": "success",
            "loop_start": f"Bar {start_bar}, Beat {start_beat}",
            "loop_end": f"Bar {end_bar}, Beat {end_beat}",
            "length_bars": end_bar - start_bar,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error setting loop region: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def duplicate_clip_in_arrangement(
    ctx: Context,
    track_index: int,
    source_bar: int,
    target_bar: int,
    length_bars: int = 4
) -> str:
    """Duplicate a section of the arrangement.

    Args:
        track_index: Track to duplicate from
        source_bar: Starting bar of the section to duplicate
        target_bar: Bar to paste the duplicated section
        length_bars: Length in bars to duplicate (default: 4)

    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)

        source_beats = (source_bar - 1) * 4
        target_beats = (target_bar - 1) * 4
        length_beats = length_bars * 4

        result = await ableton.send_command("duplicate_arrangement_region", {
            "track_index": track_index,
            "source_position": source_beats,
            "target_position": target_beats,
            "length": length_beats,
        })
        return json.dumps({
            "status": "success",
            "message": f"Duplicated {length_bars} bars from bar {source_bar} to bar {target_bar}",
        }, indent=2)
    except Exception as e:
        logger.error(f"Error duplicating arrangement: {e}")
        return json.dumps({"error": str(e)})
