"""Snapshot Tools.

Component: SVSS001A
Domain: SV (Server) | Category: SS (Tools-Snapshot)

Tools for project snapshot management:
- Create snapshots
- List snapshots
- Restore snapshots
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_snapshots

logger = logging.getLogger("sunny.tools.snapshot")


@mcp.tool()
async def create_snapshot(ctx: Context, reason: str) -> str:
    """Create a project snapshot for later recovery.

    Args:
        reason: Description of why the snapshot was created

    Returns:
        Snapshot ID that can be used for restoration
    """
    try:
        snapshots = get_snapshots(ctx)
        snapshot_id = await snapshots.create_snapshot(reason)
        return json.dumps({
            "snapshot_id": snapshot_id,
            "reason": reason,
            "message": "Snapshot created successfully"
        }, indent=2)
    except Exception as e:
        logger.error(f"Error creating snapshot: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_snapshots(ctx: Context) -> str:
    """List all available project snapshots.

    Returns:
        JSON array of snapshots with IDs, timestamps, and reasons
    """
    try:
        snapshots = get_snapshots(ctx)
        snapshot_list = await snapshots.list_snapshots()
        return json.dumps(snapshot_list, indent=2)
    except Exception as e:
        logger.error(f"Error listing snapshots: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def restore_snapshot(ctx: Context, snapshot_id: str) -> str:
    """Restore the project to a previous snapshot state.

    Args:
        snapshot_id: ID of the snapshot to restore

    Returns:
        Confirmation of restoration
    """
    try:
        snapshots = get_snapshots(ctx)
        await snapshots.restore_snapshot(snapshot_id)
        return json.dumps({
            "status": "success",
            "message": f"Restored snapshot {snapshot_id}"
        }, indent=2)
    except Exception as e:
        logger.error(f"Error restoring snapshot: {e}")
        return json.dumps({"error": str(e)})
