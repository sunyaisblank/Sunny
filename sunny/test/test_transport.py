"""Tests for the Transport Layer.

Tests for TCP/UDP communication with Ableton Live,
including connection management and offline mode.
"""

from __future__ import annotations

import pytest
import asyncio
from unittest.mock import patch, MagicMock


try:
    from Sunny.Server.transport import (
        TCPConnection,
        UDPConnection,
        AbletonConnection,
    )
    TRANSPORT_AVAILABLE = True
except ImportError:
    TRANSPORT_AVAILABLE = False


@pytest.fixture
def tcp_connection():
    """Create a TCP connection instance."""
    if not TRANSPORT_AVAILABLE:
        pytest.skip("Transport not available")
    return TCPConnection(host="127.0.0.1", port=9999, timeout=1.0)


@pytest.fixture
def udp_connection():
    """Create a UDP connection instance."""
    if not TRANSPORT_AVAILABLE:
        pytest.skip("Transport not available")
    return UDPConnection(host="127.0.0.1", port=9998)


@pytest.fixture
def ableton_connection():
    """Create an Ableton connection instance."""
    if not TRANSPORT_AVAILABLE:
        pytest.skip("Transport not available")
    return AbletonConnection()


class TestTCPConnection:
    """Tests for TCP socket connection."""
    
    def test_init_default_values(self, tcp_connection):
        """Should initialize with correct values."""
        assert tcp_connection.host == "127.0.0.1"
        assert tcp_connection.port == 9999
        assert tcp_connection.timeout == 1.0
        assert tcp_connection._socket is None
    
    def test_not_connected_by_default(self, tcp_connection):
        """Should not be connected initially."""
        assert tcp_connection._socket is None
    
    @pytest.mark.asyncio
    async def test_disconnect_when_not_connected(self, tcp_connection):
        """Disconnect should be safe when not connected."""
        await tcp_connection.disconnect()  # Should not raise
        assert tcp_connection._socket is None
    
    @pytest.mark.asyncio
    async def test_send_requires_connection(self, tcp_connection):
        """Send should fail when not connected."""
        with pytest.raises(ConnectionError):
            await tcp_connection.send("test_command")


class TestUDPConnection:
    """Tests for UDP/OSC connection."""
    
    def test_init_default_values(self, udp_connection):
        """Should initialize with correct values."""
        assert udp_connection.host == "127.0.0.1"
        assert udp_connection.port == 9998
    
    @pytest.mark.asyncio
    async def test_connect_creates_socket(self, udp_connection):
        """Connect should create a UDP socket."""
        result = await udp_connection.connect()
        assert result is True
        assert udp_connection._socket is not None
        await udp_connection.disconnect()
    
    @pytest.mark.asyncio
    async def test_disconnect_closes_socket(self, udp_connection):
        """Disconnect should close socket."""
        await udp_connection.connect()
        await udp_connection.disconnect()
        assert udp_connection._socket is None


class TestAbletonConnection:
    """Tests for the hybrid TCP/UDP connection manager."""
    
    def test_init_creates_connections(self, ableton_connection):
        """Should create TCP and UDP connection objects."""
        assert ableton_connection.tcp is not None
        assert ableton_connection.udp is not None
    
    def test_not_connected_initially(self, ableton_connection):
        """Should not be connected initially."""
        assert ableton_connection.is_connected is False
    
    @pytest.mark.asyncio
    async def test_connect_fails_gracefully(self, ableton_connection):
        """Connect should fail gracefully when Ableton not running."""
        # Without Ableton running, connection should fail but not crash
        result = await ableton_connection.connect()
        # Connection fails but doesn't raise
        assert isinstance(result, bool)
    
    @pytest.mark.asyncio
    async def test_offline_mode_returns_mock_data(self, ableton_connection):
        """Offline mode should return mock data."""
        # Don't connect, stay in offline mode
        result = await ableton_connection.send_command("get_session_info")
        
        # Should return mock data structure
        assert isinstance(result, dict)
        assert "tempo" in result
        assert "time_signature" in result
    
    @pytest.mark.asyncio
    async def test_mock_track_creation(self, ableton_connection):
        """Mock track creation should return plausible data."""
        result = await ableton_connection.send_command("create_midi_track", {
            "name": "Test Track",
            "index": 0,
        })
        
        assert isinstance(result, dict)
        assert "index" in result or "track_index" in result
    
    @pytest.mark.asyncio
    async def test_send_command_with_params(self, ableton_connection):
        """Send command should handle parameters."""
        result = await ableton_connection.send_command("get_track_info", {
            "track_index": 0,
        })
        
        assert isinstance(result, dict)
    
    @pytest.mark.asyncio
    async def test_disconnect(self, ableton_connection):
        """Disconnect should be safe."""
        await ableton_connection.disconnect()
        assert ableton_connection.is_connected is False


class TestConnectionEnvironment:
    """Tests for environment-based configuration."""
    
    def test_tcp_port_from_env(self):
        """TCP port should be configurable via environment."""
        if not TRANSPORT_AVAILABLE:
            pytest.skip("Transport not available")
        
        with patch.dict("os.environ", {"SUNNY_TCP_PORT": "12345"}):
            conn = TCPConnection()
            assert conn.port == 12345
    
    def test_udp_port_from_env(self):
        """UDP port should be configurable via environment."""
        if not TRANSPORT_AVAILABLE:
            pytest.skip("Transport not available")
        
        with patch.dict("os.environ", {"SUNNY_UDP_PORT": "12346"}):
            conn = UDPConnection()
            assert conn.port == 12346
