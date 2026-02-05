"""Track Tools.

Component: SVTT001A
Domain: SV (Server) | Category: TT (Tools-Track)

Tools for track creation and management:
- Create MIDI/Audio tracks
- Delete tracks (with snapshot safety)
- Get track information
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton, get_snapshots, check_rate_limit
from sunny.infrastructure.audit import (
    AuditEntry,
    ActionCategory,
    Outcome,
    Severity,
    audit_log,
)
from sunny.infrastructure.security import ValidationError, validate_track_index

logger = logging.getLogger("sunny.tools.track")


@mcp.tool()
async def create_midi_track(
    ctx: Context,
    name: str | None = None,
    index: int = -1
) -> str:
    """Create a new MIDI track in the session.

    Args:
        name: Optional name for the track
        index: Position to insert (-1 = end of track list)

    Returns:
        JSON with created track info including index and name
    """
    try:
        ableton = get_ableton(ctx)
        params = {"index": index}
        if name:
            params["name"] = name

        result = await ableton.send_command("create_midi_track", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating MIDI track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_audio_track(
    ctx: Context,
    name: str | None = None,
    index: int = -1
) -> str:
    """Create a new audio track in the session.

    Args:
        name: Optional name for the track
        index: Position to insert (-1 = end of track list)

    Returns:
        JSON with created track info including index and name
    """
    try:
        ableton = get_ableton(ctx)
        params = {"index": index}
        if name:
            params["name"] = name

        result = await ableton.send_command("create_audio_track", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating audio track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def delete_track(ctx: Context, track_index: int) -> str:
    """Delete a track from the session.

    SAFETY: Automatically creates a project snapshot before deletion.

    Args:
        track_index: Index of the track to delete

    Returns:
        Confirmation with snapshot ID for recovery
    """
    try:
        # Rate limit check
        check_rate_limit(ctx)

        # Input validation
        validated_index = validate_track_index(track_index, "track_index")

        # Audit log for destructive operation
        audit_log(
            AuditEntry(
                action="DELETE_TRACK",
                entity_type="Track",
                entity_id=str(validated_index),
                description=f"Deleting track {validated_index}",
                category=ActionCategory.TOOL,
                severity=Severity.WARNING,
            )
        )

        # Create snapshot before destructive operation
        snapshots = get_snapshots(ctx)
        snapshot_id = await snapshots.create_snapshot(f"Before delete track {validated_index}")

        ableton = get_ableton(ctx)
        result = await ableton.send_command("delete_track", {"track_index": validated_index})

        audit_log(
            AuditEntry(
                action="DELETE_TRACK",
                entity_type="Track",
                entity_id=str(validated_index),
                description=f"Track {validated_index} deleted successfully",
                category=ActionCategory.TOOL,
                outcome=Outcome.SUCCESS,
                metadata={"snapshot_id": snapshot_id},
            )
        )

        return json.dumps({
            "status": "success",
            "message": f"Deleted track {validated_index}",
            "snapshot_id": snapshot_id,
            "recovery": f"Use restore_snapshot('{snapshot_id}') to undo"
        }, indent=2)
    except ValidationError as e:
        logger.warning(f"Validation error in delete_track: {e}")
        return json.dumps({"error": str(e)})
    except Exception as e:
        logger.error(f"Error deleting track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_track_info(ctx: Context, track_index: int) -> str:
    """Get detailed information about a specific track.

    Args:
        track_index: Index of the track to query

    Returns:
        JSON with track details including:
        - Name
        - Type (MIDI/Audio)
        - Volume/Pan
        - Mute/Solo/Arm state
        - Devices loaded
        - Clip slots
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_track_info", {"track_index": track_index})
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting track info: {e}")
        return json.dumps({"error": str(e)})
