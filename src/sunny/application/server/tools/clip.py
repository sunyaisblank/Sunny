"""Clip Tools.

Component: SVTC001A
Domain: SV (Server) | Category: TC (Tools-Clip)

Tools for clip creation and note manipulation:
- Create MIDI clips
- Add notes to clips
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.clip")


@mcp.tool()
async def create_clip(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    length_beats: float = 4.0,
    name: str | None = None
) -> str:
    """Create a new MIDI clip in the specified track and slot.

    Args:
        track_index: Index of the MIDI track
        clip_slot: Clip slot index (row in session view)
        length_beats: Length of the clip in beats (default: 4.0)
        name: Optional name for the clip

    Returns:
        JSON with created clip info
    """
    try:
        ableton = get_ableton(ctx)
        params = {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "length": length_beats,
        }
        if name:
            params["name"] = name

        result = await ableton.send_command("create_clip", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating clip: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def add_notes_to_clip(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    notes: list[dict]
) -> str:
    """Add MIDI notes to an existing clip.

    Args:
        track_index: Index of the track containing the clip
        clip_slot: Clip slot index
        notes: List of note dictionaries, each containing:
            - pitch: MIDI note number (0-127)
            - start_time: Start position in beats
            - duration: Note length in beats
            - velocity: Note velocity (1-127, default: 100)
            - mute: Whether note is muted (default: false)

    Returns:
        Confirmation with number of notes added

    Example:
        add_notes_to_clip(0, 0, [
            {"pitch": 60, "start_time": 0, "duration": 1, "velocity": 100},
            {"pitch": 64, "start_time": 1, "duration": 1, "velocity": 90},
        ])
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("add_notes_to_clip", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "notes": notes,
        })
        return f"Added {len(notes)} notes to clip at track {track_index}, slot {clip_slot}"
    except Exception as e:
        logger.error(f"Error adding notes to clip: {e}")
        return json.dumps({"error": str(e)})
