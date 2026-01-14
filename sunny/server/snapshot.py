"""Sunny - Project Snapshot System.

Component: SRSN001A
Domain: SR (Server) | Category: SN (Snapshot)

Provides automatic backup before destructive operations
to enable Creative Undo functionality.

Bounds:
    - max_snapshots: int ∈ [1, 500]
    - snapshot_id: str (8-char UUID prefix)

Error Codes:
    - 1150: Snapshot creation failed
    - 1151: Snapshot not found
    - 1152: Restore failed
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any

logger = logging.getLogger("sunny.snapshot")


class SnapshotManager:
    """Manages project snapshots for safe destructive operations.
    
    Component: SRSN001A
    
    Creates timestamped snapshots before any operation that could
    result in data loss, enabling easy recovery.
    
    Bounds:
        - max_snapshots: int ∈ [1, 500]
        - snapshot_id: str (8-char UUID prefix)
    
    Error Codes:
        - 1150: Snapshot creation failed
        - 1151: Snapshot not found
        - 1152: Restore failed
    """
    
    def __init__(
        self,
        snapshot_dir: Path | None = None,
        max_snapshots: int | None = None
    ):
        """Initialize snapshot manager.
        
        Args:
            snapshot_dir: Directory for snapshot storage
            max_snapshots: Maximum number of snapshots to retain
        """
        # Get configuration from environment
        default_dir = Path.home() / ".sunny" / "snapshots"
        self.snapshot_dir = snapshot_dir or Path(
            os.getenv("SUNNY_SNAPSHOT_DIR", str(default_dir))
        )
        self.max_snapshots = max_snapshots or int(
            os.getenv("SUNNY_MAX_SNAPSHOTS", "50")
        )
        
        # Ensure directory exists
        self.snapshot_dir.mkdir(parents=True, exist_ok=True)
        
        # In-memory index of snapshots
        self._index: list[dict[str, Any]] = []
        self._load_index()
    
    def _load_index(self):
        """Load snapshot index from disk."""
        index_file = self.snapshot_dir / "index.json"
        if index_file.exists():
            try:
                with open(index_file, "r") as f:
                    self._index = json.load(f)
            except (json.JSONDecodeError, IOError) as e:
                logger.warning(f"Could not load snapshot index: {e}")
                self._index = []
    
    def _save_index(self):
        """Save snapshot index to disk."""
        index_file = self.snapshot_dir / "index.json"
        try:
            with open(index_file, "w") as f:
                json.dump(self._index, f, indent=2)
        except IOError as e:
            logger.error(f"Could not save snapshot index: {e}")
    
    async def create_snapshot(self, reason: str) -> str:
        """Create a new project snapshot.
        
        Args:
            reason: Description of why the snapshot was created
        
        Returns:
            Unique snapshot ID
        """
        snapshot_id = str(uuid.uuid4())[:8]
        timestamp = datetime.now().isoformat()
        
        snapshot_entry = {
            "id": snapshot_id,
            "timestamp": timestamp,
            "reason": reason,
            "file": f"{snapshot_id}.snapshot",
        }
        
        # TODO: Implement actual Ableton project state capture
        # For now, we just record the metadata
        snapshot_file = self.snapshot_dir / f"{snapshot_id}.snapshot"
        snapshot_data = {
            "id": snapshot_id,
            "timestamp": timestamp,
            "reason": reason,
            # Placeholder for actual project state
            "state": {},
        }
        
        try:
            with open(snapshot_file, "w") as f:
                json.dump(snapshot_data, f, indent=2)
        except IOError as e:
            logger.error(f"Could not save snapshot: {e}")
            raise RuntimeError(f"Failed to create snapshot: {e}")
        
        # Add to index
        self._index.append(snapshot_entry)
        
        # Enforce retention policy
        await self._enforce_retention()
        
        # Save index
        self._save_index()
        
        logger.info(f"Created snapshot {snapshot_id}: {reason}")
        return snapshot_id
    
    async def list_snapshots(self) -> list[dict[str, Any]]:
        """List all available snapshots.
        
        Returns:
            List of snapshot metadata dictionaries
        """
        return [
            {
                "id": s["id"],
                "timestamp": s["timestamp"],
                "reason": s["reason"],
            }
            for s in sorted(self._index, key=lambda x: x["timestamp"], reverse=True)
        ]
    
    async def get_snapshot(self, snapshot_id: str) -> dict[str, Any] | None:
        """Get snapshot metadata by ID.
        
        Args:
            snapshot_id: Snapshot ID to look up
        
        Returns:
            Snapshot metadata or None if not found
        """
        for snapshot in self._index:
            if snapshot["id"] == snapshot_id:
                return snapshot
        return None
    
    async def restore_snapshot(self, snapshot_id: str):
        """Restore project to a snapshot state.
        
        Args:
            snapshot_id: ID of the snapshot to restore
        
        Raises:
            ValueError: If snapshot not found
        """
        snapshot = await self.get_snapshot(snapshot_id)
        if not snapshot:
            raise ValueError(f"Snapshot {snapshot_id} not found")
        
        snapshot_file = self.snapshot_dir / snapshot["file"]
        if not snapshot_file.exists():
            raise ValueError(f"Snapshot file missing for {snapshot_id}")
        
        try:
            with open(snapshot_file, "r") as f:
                snapshot_data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            raise RuntimeError(f"Could not read snapshot: {e}")
        
        # TODO: Implement actual project state restoration
        # This would involve sending commands to Ableton to restore
        # tracks, clips, device states, etc.
        
        logger.info(f"Restored snapshot {snapshot_id}")
    
    async def delete_snapshot(self, snapshot_id: str):
        """Delete a snapshot.
        
        Args:
            snapshot_id: ID of the snapshot to delete
        """
        snapshot = await self.get_snapshot(snapshot_id)
        if not snapshot:
            return
        
        # Remove file
        snapshot_file = self.snapshot_dir / snapshot["file"]
        if snapshot_file.exists():
            try:
                snapshot_file.unlink()
            except IOError as e:
                logger.warning(f"Could not delete snapshot file: {e}")
        
        # Remove from index
        self._index = [s for s in self._index if s["id"] != snapshot_id]
        self._save_index()
        
        logger.info(f"Deleted snapshot {snapshot_id}")
    
    async def _enforce_retention(self):
        """Enforce snapshot retention policy.
        
        Removes oldest snapshots if count exceeds maximum.
        """
        while len(self._index) > self.max_snapshots:
            # Sort by timestamp, remove oldest
            sorted_snapshots = sorted(self._index, key=lambda x: x["timestamp"])
            oldest = sorted_snapshots[0]
            await self.delete_snapshot(oldest["id"])
