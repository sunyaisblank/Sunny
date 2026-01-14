"""Tests for the Snapshot System.

Tests for project snapshot creation, listing, restoration,
and retention policy.
"""

from __future__ import annotations

import pytest
import asyncio
import tempfile
import shutil
from pathlib import Path


try:
    from Sunny.Server.snapshot import SnapshotManager
    SNAPSHOT_AVAILABLE = True
except ImportError:
    SNAPSHOT_AVAILABLE = False


@pytest.fixture
def temp_snapshot_dir():
    """Create a temporary directory for snapshots."""
    temp_dir = Path(tempfile.mkdtemp())
    yield temp_dir
    shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.fixture
def snapshot_manager(temp_snapshot_dir):
    """Create a snapshot manager with temp directory."""
    if not SNAPSHOT_AVAILABLE:
        pytest.skip("Snapshot manager not available")
    return SnapshotManager(snapshot_dir=temp_snapshot_dir, max_snapshots=5)


class TestSnapshotCreation:
    """Tests for snapshot creation."""
    
    @pytest.mark.asyncio
    async def test_create_snapshot_returns_id(self, snapshot_manager):
        """Create should return a snapshot ID."""
        snapshot_id = await snapshot_manager.create_snapshot("Test snapshot")
        
        assert snapshot_id is not None
        assert len(snapshot_id) == 8  # UUID first 8 chars
    
    @pytest.mark.asyncio
    async def test_snapshot_file_created(self, snapshot_manager, temp_snapshot_dir):
        """Snapshot file should be created on disk."""
        snapshot_id = await snapshot_manager.create_snapshot("Test")
        
        snapshot_file = temp_snapshot_dir / f"{snapshot_id}.snapshot"
        assert snapshot_file.exists()
    
    @pytest.mark.asyncio
    async def test_index_updated(self, snapshot_manager, temp_snapshot_dir):
        """Index file should be updated."""
        await snapshot_manager.create_snapshot("Test")
        
        index_file = temp_snapshot_dir / "index.json"
        assert index_file.exists()
    
    @pytest.mark.asyncio
    async def test_multiple_snapshots_unique_ids(self, snapshot_manager):
        """Multiple snapshots should have unique IDs."""
        id1 = await snapshot_manager.create_snapshot("First")
        id2 = await snapshot_manager.create_snapshot("Second")
        id3 = await snapshot_manager.create_snapshot("Third")
        
        assert len({id1, id2, id3}) == 3


class TestSnapshotListing:
    """Tests for snapshot listing."""
    
    @pytest.mark.asyncio
    async def test_list_empty(self, snapshot_manager):
        """List should be empty initially."""
        snapshots = await snapshot_manager.list_snapshots()
        assert snapshots == []
    
    @pytest.mark.asyncio
    async def test_list_after_creation(self, snapshot_manager):
        """List should include created snapshots."""
        await snapshot_manager.create_snapshot("Test 1")
        await snapshot_manager.create_snapshot("Test 2")
        
        snapshots = await snapshot_manager.list_snapshots()
        
        assert len(snapshots) == 2
        reasons = [s["reason"] for s in snapshots]
        assert "Test 1" in reasons
        assert "Test 2" in reasons
    
    @pytest.mark.asyncio
    async def test_list_sorted_by_timestamp(self, snapshot_manager):
        """List should be sorted newest first."""
        await snapshot_manager.create_snapshot("First")
        await asyncio.sleep(0.01)  # Ensure different timestamps
        await snapshot_manager.create_snapshot("Second")
        
        snapshots = await snapshot_manager.list_snapshots()
        
        assert snapshots[0]["reason"] == "Second"
        assert snapshots[1]["reason"] == "First"


class TestSnapshotRetrieval:
    """Tests for getting individual snapshots."""
    
    @pytest.mark.asyncio
    async def test_get_existing_snapshot(self, snapshot_manager):
        """Should retrieve existing snapshot."""
        snapshot_id = await snapshot_manager.create_snapshot("Test")
        
        snapshot = await snapshot_manager.get_snapshot(snapshot_id)
        
        assert snapshot is not None
        assert snapshot["id"] == snapshot_id
        assert snapshot["reason"] == "Test"
    
    @pytest.mark.asyncio
    async def test_get_nonexistent_snapshot(self, snapshot_manager):
        """Should return None for nonexistent snapshot."""
        snapshot = await snapshot_manager.get_snapshot("nonexistent")
        assert snapshot is None


class TestSnapshotDeletion:
    """Tests for snapshot deletion."""
    
    @pytest.mark.asyncio
    async def test_delete_snapshot(self, snapshot_manager, temp_snapshot_dir):
        """Should delete snapshot and file."""
        snapshot_id = await snapshot_manager.create_snapshot("Test")
        snapshot_file = temp_snapshot_dir / f"{snapshot_id}.snapshot"
        
        assert snapshot_file.exists()
        
        await snapshot_manager.delete_snapshot(snapshot_id)
        
        assert not snapshot_file.exists()
        snapshot = await snapshot_manager.get_snapshot(snapshot_id)
        assert snapshot is None
    
    @pytest.mark.asyncio
    async def test_delete_nonexistent_safe(self, snapshot_manager):
        """Deleting nonexistent snapshot should not error."""
        await snapshot_manager.delete_snapshot("nonexistent")  # Should not raise


class TestRetentionPolicy:
    """Tests for snapshot retention policy."""
    
    @pytest.mark.asyncio
    async def test_retention_enforced(self, temp_snapshot_dir):
        """Should delete old snapshots when exceeding max."""
        if not SNAPSHOT_AVAILABLE:
            pytest.skip("Snapshot manager not available")
        
        manager = SnapshotManager(snapshot_dir=temp_snapshot_dir, max_snapshots=3)
        
        # Create more than max
        ids = []
        for i in range(5):
            ids.append(await manager.create_snapshot(f"Snapshot {i}"))
            await asyncio.sleep(0.01)  # Ensure different timestamps
        
        snapshots = await manager.list_snapshots()
        
        # Should only have max_snapshots
        assert len(snapshots) == 3
        
        # Oldest should be deleted
        existing_ids = [s["id"] for s in snapshots]
        assert ids[0] not in existing_ids  # First is oldest, should be gone
        assert ids[1] not in existing_ids  # Second oldest, should be gone


class TestSnapshotRestoration:
    """Tests for snapshot restoration."""
    
    @pytest.mark.asyncio
    async def test_restore_existing_snapshot(self, snapshot_manager):
        """Should restore without error."""
        snapshot_id = await snapshot_manager.create_snapshot("Test")
        
        # Should not raise
        await snapshot_manager.restore_snapshot(snapshot_id)
    
    @pytest.mark.asyncio
    async def test_restore_nonexistent_raises(self, snapshot_manager):
        """Should raise for nonexistent snapshot."""
        with pytest.raises(ValueError, match="not found"):
            await snapshot_manager.restore_snapshot("nonexistent")


class TestSnapshotPersistence:
    """Tests for snapshot persistence across instances."""
    
    @pytest.mark.asyncio
    async def test_snapshots_persist(self, temp_snapshot_dir):
        """Snapshots should persist across manager instances."""
        if not SNAPSHOT_AVAILABLE:
            pytest.skip("Snapshot manager not available")
        
        # Create with first manager
        manager1 = SnapshotManager(snapshot_dir=temp_snapshot_dir)
        snapshot_id = await manager1.create_snapshot("Persistent")
        
        # Load with second manager
        manager2 = SnapshotManager(snapshot_dir=temp_snapshot_dir)
        snapshot = await manager2.get_snapshot(snapshot_id)
        
        assert snapshot is not None
        assert snapshot["reason"] == "Persistent"
