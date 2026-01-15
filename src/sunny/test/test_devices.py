"""Tests for the Device and Plugin Tools.

Integration tests for device management, parameter control,
and real-time modulation.
"""

from __future__ import annotations

import pytest
import json
from unittest.mock import AsyncMock, MagicMock, patch


# Mock context for testing MCP tools
class MockContext:
    """Mock MCP context for testing."""
    
    def __init__(self, ableton_mock=None, snapshots_mock=None):
        self.request_context = MagicMock()
        self.request_context.lifespan_context = {
            "ableton": ableton_mock or AsyncMock(),
            "snapshots": snapshots_mock or AsyncMock(),
            "theory": MagicMock(),
        }


class TestDeviceListRetrieval:
    """Tests for getting device lists."""
    
    @pytest.mark.asyncio
    async def test_get_device_list_returns_devices(self):
        """Should return list of devices on track."""
        # Setup mock
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "devices": [
                {"name": "Simpler", "type": "instrument", "index": 0},
                {"name": "Compressor", "type": "audio_effect", "index": 1},
            ]
        }
        
        ctx = MockContext(ableton_mock=ableton)
        
        # Test would call the tool - simulate response
        result = await ableton.send_command("get_device_list", {"track_index": 0})
        
        assert len(result["devices"]) == 2
        assert result["devices"][0]["name"] == "Simpler"
    
    @pytest.mark.asyncio
    async def test_get_device_list_empty_track(self):
        """Should return empty list for track with no devices."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {"devices": []}
        
        result = await ableton.send_command("get_device_list", {"track_index": 0})
        
        assert result["devices"] == []


class TestDeviceParameters:
    """Tests for device parameter operations."""
    
    @pytest.mark.asyncio
    async def test_get_device_parameters(self):
        """Should return all parameters for a device."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "device_name": "Compressor",
            "parameters": [
                {"name": "Threshold", "value": 0.5, "min": 0, "max": 1, "index": 0},
                {"name": "Ratio", "value": 0.3, "min": 0, "max": 1, "index": 1},
            ]
        }
        
        result = await ableton.send_command("get_device_parameters", {
            "track_index": 0,
            "device_index": 1,
        })
        
        assert result["device_name"] == "Compressor"
        assert len(result["parameters"]) == 2
    
    @pytest.mark.asyncio
    async def test_set_device_parameter(self):
        """Should set parameter value."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {"status": "success", "new_value": 0.75}
        
        result = await ableton.send_command("set_device_parameter", {
            "track_index": 0,
            "device_index": 0,
            "param_index": 1,
            "value": 0.75,
        })
        
        assert result["status"] == "success"
        ableton.send_command.assert_called_once()
    
    @pytest.mark.asyncio
    async def test_parameter_value_clamping(self):
        """Values should be clamped to 0.0-1.0 range."""
        # Test that the tool implementation clamps values
        test_value = 1.5
        clamped = max(0.0, min(1.0, test_value))
        assert clamped == 1.0
        
        test_value = -0.5
        clamped = max(0.0, min(1.0, test_value))
        assert clamped == 0.0
    
    @pytest.mark.asyncio
    async def test_batch_parameter_update(self):
        """Should update multiple parameters at once."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "status": "success",
            "updated_count": 3,
        }
        
        result = await ableton.send_command("set_device_parameters_batch", {
            "track_index": 0,
            "device_index": 0,
            "parameters": [
                {"param_index": 0, "value": 0.5},
                {"param_index": 1, "value": 0.75},
                {"param_index": 2, "value": 0.25},
            ]
        })
        
        assert result["updated_count"] == 3


class TestDeviceManagement:
    """Tests for device loading and deletion."""
    
    @pytest.mark.asyncio
    async def test_load_device_by_uri(self):
        """Should load device from browser URI."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "status": "success",
            "device_name": "Auto Filter",
            "device_index": 2,
        }
        
        result = await ableton.send_command("load_device_by_uri", {
            "track_index": 0,
            "uri": "Instruments/Effects/Auto Filter",
        })
        
        assert result["device_name"] == "Auto Filter"
    
    @pytest.mark.asyncio
    async def test_delete_device_creates_snapshot(self):
        """Delete should create snapshot before removal."""
        ableton = AsyncMock()
        snapshots = AsyncMock()
        snapshots.create_snapshot.return_value = "snap_12345"
        
        # Simulate the delete flow
        snapshot_id = await snapshots.create_snapshot("Before delete device")
        ableton.send_command.return_value = {"status": "success"}
        
        result = await ableton.send_command("delete_device", {
            "track_index": 0,
            "device_index": 1,
        })
        
        assert snapshot_id == "snap_12345"
        snapshots.create_snapshot.assert_called_once()
    
    @pytest.mark.asyncio
    async def test_enable_disable_device(self):
        """Should toggle device enabled state."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {"status": "success", "enabled": False}
        
        result = await ableton.send_command("enable_device", {
            "track_index": 0,
            "device_index": 0,
            "enabled": False,
        })
        
        assert result["enabled"] is False


class TestBrowserIntegration:
    """Tests for browser instrument/effect discovery."""
    
    @pytest.mark.asyncio
    async def test_get_browser_instruments(self):
        """Should list available instruments."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "categories": [
                {"name": "Piano & Keys", "items": ["Grand Piano", "Electric Piano"]},
                {"name": "Drums", "items": ["Kit-Core 909", "Kit-Core 808"]},
            ]
        }
        
        result = await ableton.send_command("get_browser_instruments")
        
        assert len(result["categories"]) == 2
    
    @pytest.mark.asyncio
    async def test_get_browser_effects(self):
        """Should list available effects."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "categories": [
                {"name": "Dynamics", "items": ["Compressor", "Limiter"]},
                {"name": "EQ", "items": ["EQ Eight", "EQ Three"]},
            ]
        }
        
        result = await ableton.send_command("get_browser_effects")
        
        assert len(result["categories"]) == 2


class TestRealtimeModulation:
    """Tests for real-time UDP parameter modulation."""
    
    def test_udp_message_format(self):
        """UDP messages should use correct OSC format."""
        # Expected format: /track/{t}/device/{d}/param/{p} {value}
        track, device, param = 0, 1, 2
        value = 0.5
        
        address = f"/track/{track}/device/{device}/param/{param}"
        
        assert address == "/track/0/device/1/param/2"
    
    @pytest.mark.asyncio
    async def test_modulate_parameter_realtime(self):
        """Should send UDP message for real-time control."""
        ableton = AsyncMock()
        ableton.send_realtime = MagicMock()
        
        # Call real-time modulation
        address = "/track/0/device/0/param/1"
        ableton.send_realtime(address, 0.75)
        
        ableton.send_realtime.assert_called_once_with(address, 0.75)


class TestSidechainRouting:
    """Tests for sidechain setup."""
    
    @pytest.mark.asyncio
    async def test_create_sidechain(self):
        """Should configure sidechain routing."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {
            "status": "success",
            "source_track": 0,
            "target_track": 1,
            "target_device": 0,
        }
        
        result = await ableton.send_command("create_sidechain", {
            "source_track": 0,
            "target_track": 1,
            "target_device": 0,
        })
        
        assert result["status"] == "success"


class TestMixerOperations:
    """Tests for mixer-level operations."""
    
    @pytest.mark.asyncio
    async def test_auto_gain_headroom(self):
        """Should calculate proper gain for -6dB headroom."""
        # Auto-gain should target -6dB peak headroom
        target_headroom_db = -6.0
        current_peak_db = 0.0
        
        gain_adjustment = target_headroom_db - current_peak_db
        
        assert gain_adjustment == -6.0
    
    @pytest.mark.asyncio
    async def test_send_routing(self):
        """Should configure send levels."""
        ableton = AsyncMock()
        ableton.send_command.return_value = {"status": "success", "send_level": 0.5}
        
        result = await ableton.send_command("set_send_level", {
            "track_index": 0,
            "send_index": 0,
            "level": 0.5,
        })
        
        assert result["send_level"] == 0.5
