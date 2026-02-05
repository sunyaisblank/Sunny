"""Integration tests for snapshot manager.

Tests the snapshot system for project state backup.
"""

from __future__ import annotations

import json
import pytest


class TestSnapshotManager:
    """Test SnapshotManager class."""

    def test_manager_creation(self, snapshot_dir):
        """Verify SnapshotManager can be created."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        assert manager.snapshot_dir == snapshot_dir
        assert manager.max_snapshots > 0

    @pytest.mark.asyncio
    async def test_create_snapshot(self, snapshot_dir):
        """Verify snapshot creation."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        snapshot_id = await manager.create_snapshot("Test snapshot")

        assert snapshot_id is not None
        assert len(snapshot_id) == 8  # UUID prefix

        # Snapshot file should exist
        snapshot_files = list(snapshot_dir.glob("*.snapshot"))
        assert len(snapshot_files) == 1

    @pytest.mark.asyncio
    async def test_list_snapshots(self, snapshot_dir):
        """Verify snapshot listing."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        # Create multiple snapshots
        await manager.create_snapshot("First")
        await manager.create_snapshot("Second")
        await manager.create_snapshot("Third")

        snapshots = await manager.list_snapshots()

        assert len(snapshots) == 3
        # Should be sorted by timestamp (newest first)
        assert snapshots[0]["reason"] == "Third"

    @pytest.mark.asyncio
    async def test_get_snapshot(self, snapshot_dir):
        """Verify snapshot retrieval."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        snapshot_id = await manager.create_snapshot("Test get")

        snapshot = await manager.get_snapshot(snapshot_id)

        assert snapshot is not None
        assert snapshot["id"] == snapshot_id
        assert snapshot["reason"] == "Test get"

    @pytest.mark.asyncio
    async def test_get_nonexistent_snapshot(self, snapshot_dir):
        """Verify handling of nonexistent snapshot."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        snapshot = await manager.get_snapshot("nonexist")

        assert snapshot is None

    @pytest.mark.asyncio
    async def test_delete_snapshot(self, snapshot_dir):
        """Verify snapshot deletion."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        snapshot_id = await manager.create_snapshot("To delete")
        await manager.delete_snapshot(snapshot_id)

        snapshot = await manager.get_snapshot(snapshot_id)
        assert snapshot is None

        # File should be removed
        snapshot_files = list(snapshot_dir.glob("*.snapshot"))
        assert len(snapshot_files) == 0

    @pytest.mark.asyncio
    async def test_retention_policy(self, snapshot_dir):
        """Verify retention policy enforcement."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir, max_snapshots=3)

        # Create more than max
        for i in range(5):
            await manager.create_snapshot(f"Snapshot {i}")

        snapshots = await manager.list_snapshots()

        # Should only keep max_snapshots
        assert len(snapshots) <= 3


class TestSnapshotContent:
    """Test snapshot content structure."""

    @pytest.mark.asyncio
    async def test_snapshot_file_structure(self, snapshot_dir):
        """Verify snapshot file has correct structure."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)

        snapshot_id = await manager.create_snapshot("Structure test")

        # Read the snapshot file
        snapshot_file = snapshot_dir / f"{snapshot_id}.snapshot"
        with open(snapshot_file) as f:
            data = json.load(f)

        assert "id" in data
        assert "timestamp" in data
        assert "reason" in data
        assert "state" in data

    @pytest.mark.asyncio
    async def test_offline_snapshot(self, snapshot_dir):
        """Verify offline snapshot (no transport) works."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)
        # No transport set - should work in offline mode

        snapshot_id = await manager.create_snapshot("Offline snapshot")

        snapshot_file = snapshot_dir / f"{snapshot_id}.snapshot"
        with open(snapshot_file) as f:
            data = json.load(f)

        # State should be empty in offline mode
        assert data["state"] == {}


class TestSnapshotIndex:
    """Test snapshot index management."""

    @pytest.mark.asyncio
    async def test_index_persistence(self, snapshot_dir):
        """Verify index is persisted to disk."""
        from sunny.server.snapshot import SnapshotManager

        manager = SnapshotManager(snapshot_dir=snapshot_dir)
        await manager.create_snapshot("Index test")

        index_file = snapshot_dir / "index.json"
        assert index_file.exists()

        with open(index_file) as f:
            index = json.load(f)

        assert len(index) == 1

    @pytest.mark.asyncio
    async def test_index_reload(self, snapshot_dir):
        """Verify index is reloaded on manager creation."""
        from sunny.server.snapshot import SnapshotManager

        # First manager creates snapshot
        manager1 = SnapshotManager(snapshot_dir=snapshot_dir)
        await manager1.create_snapshot("Reload test")

        # Second manager should see it
        manager2 = SnapshotManager(snapshot_dir=snapshot_dir)
        snapshots = await manager2.list_snapshots()

        assert len(snapshots) == 1
