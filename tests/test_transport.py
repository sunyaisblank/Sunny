"""Integration tests for transport module.

Tests the TCP/UDP transport for Ableton connection.
"""

from __future__ import annotations

import asyncio
import json
import pytest


class TestTransportConfig:
    """Test TransportConfig class."""

    def test_default_config(self):
        """Verify default configuration values."""
        from sunny.host.transport import TransportConfig

        config = TransportConfig()

        assert config.host == "127.0.0.1"
        assert config.tcp_port == 9001
        assert config.udp_port == 9002
        assert config.timeout_seconds == 5.0
        assert config.retry_count == 3

    def test_config_from_env(self, monkeypatch):
        """Verify configuration from environment."""
        from sunny.host.transport import TransportConfig

        monkeypatch.setenv("SUNNY_ABLETON_HOST", "192.168.1.100")
        monkeypatch.setenv("SUNNY_ABLETON_TCP_PORT", "8001")
        monkeypatch.setenv("SUNNY_ABLETON_UDP_PORT", "8002")

        config = TransportConfig.from_env()

        assert config.host == "192.168.1.100"
        assert config.tcp_port == 8001
        assert config.udp_port == 8002


class TestConnectionState:
    """Test ConnectionState enum."""

    def test_states_exist(self):
        """Verify all expected states exist."""
        from sunny.host.transport import ConnectionState

        assert hasattr(ConnectionState, "DISCONNECTED")
        assert hasattr(ConnectionState, "CONNECTING")
        assert hasattr(ConnectionState, "CONNECTED")
        assert hasattr(ConnectionState, "RECONNECTING")
        assert hasattr(ConnectionState, "ERROR")


class TestTcpTransport:
    """Test TcpTransport class."""

    def test_creation(self):
        """Verify TcpTransport can be created."""
        from sunny.host.transport import TcpTransport, ConnectionState

        transport = TcpTransport(host="127.0.0.1", port=9001)

        assert transport.host == "127.0.0.1"
        assert transport.port == 9001
        assert transport.state == ConnectionState.DISCONNECTED
        assert not transport.is_connected

    def test_state_listener(self):
        """Verify state listener callback."""
        from sunny.host.transport import TcpTransport, ConnectionState

        states = []

        class Listener:
            def on_state_change(self, old, new, msg):
                states.append((old, new, msg))

        transport = TcpTransport()
        transport.add_state_listener(Listener())

        # Manually trigger state change
        transport._set_state(ConnectionState.CONNECTING, "Test")

        assert len(states) == 1
        assert states[0][1] == ConnectionState.CONNECTING

    def test_message_queue(self):
        """Verify message queuing."""
        from sunny.host.transport import TcpTransport

        transport = TcpTransport(max_queue_size=5)

        # Queue messages
        for i in range(3):
            transport._queue_message(f"message_{i}")

        assert transport.queue_size == 3

        # Queue beyond limit
        for i in range(10):
            transport._queue_message(f"overflow_{i}")

        # Should be capped at max_queue_size
        assert transport.queue_size == 5


class TestUdpTransport:
    """Test UdpTransport class."""

    def test_creation(self):
        """Verify UdpTransport can be created."""
        from sunny.host.transport import UdpTransport

        transport = UdpTransport(host="127.0.0.1", port=9002)

        assert transport.host == "127.0.0.1"
        assert transport.port == 9002
        assert not transport.is_connected


class TestAbletonConnection:
    """Test AbletonConnection class."""

    def test_creation(self):
        """Verify AbletonConnection can be created."""
        from sunny.host.transport import AbletonConnection, TransportConfig

        config = TransportConfig(host="127.0.0.1", tcp_port=9001)
        conn = AbletonConnection(config)

        assert conn.config.host == "127.0.0.1"
        assert not conn.is_connected

    def test_status(self):
        """Verify status reporting."""
        from sunny.host.transport import AbletonConnection

        conn = AbletonConnection()
        status = conn.get_status()

        assert "state" in status
        assert "is_connected" in status
        assert "host" in status
        assert "tcp_port" in status
        assert "queue_size" in status
        assert status["is_connected"] is False

    @pytest.mark.asyncio
    async def test_connect_no_server(self):
        """Verify connect fails gracefully when no server."""
        from sunny.host.transport import AbletonConnection, TransportConfig

        # Use a port that's unlikely to have a server
        config = TransportConfig(
            host="127.0.0.1",
            tcp_port=59999,
            timeout_seconds=0.5,
            retry_count=1,
        )
        conn = AbletonConnection(config)

        # Should fail but not raise
        result = await conn.connect()
        assert result is False

    @pytest.mark.asyncio
    async def test_send_not_connected(self):
        """Verify send returns error when not connected."""
        from sunny.host.transport import AbletonConnection

        conn = AbletonConnection()

        result = await conn.send_command("test")

        assert "error" in result
        assert "Not connected" in result["error"]


class TestConnectionIntegration:
    """Integration tests requiring a mock server."""

    @pytest.fixture
    async def mock_server(self):
        """Create a mock TCP server for testing."""
        connections = []
        responses = {}

        async def handle_client(reader, writer):
            connections.append((reader, writer))
            try:
                while True:
                    data = await reader.readline()
                    if not data:
                        break

                    try:
                        request = json.loads(data.decode())
                        cmd = request.get("command", "")

                        # Generate response
                        if cmd in responses:
                            response = responses[cmd]
                        else:
                            response = {
                                "id": request.get("id"),
                                "result": "ok",
                            }

                        writer.write((json.dumps(response) + "\n").encode())
                        await writer.drain()
                    except json.JSONDecodeError:
                        pass
            except asyncio.CancelledError:
                pass
            finally:
                writer.close()

        server = await asyncio.start_server(handle_client, "127.0.0.1", 0)
        port = server.sockets[0].getsockname()[1]

        yield {"server": server, "port": port, "responses": responses}

        server.close()
        await server.wait_closed()

    @pytest.mark.asyncio
    async def test_connect_to_server(self, mock_server):
        """Verify connection to mock server."""
        from sunny.host.transport import AbletonConnection, TransportConfig

        config = TransportConfig(
            host="127.0.0.1",
            tcp_port=mock_server["port"],
            timeout_seconds=1.0,
        )
        conn = AbletonConnection(config)

        try:
            result = await conn.connect()
            assert result is True
            assert conn.is_connected
        finally:
            await conn.disconnect()

    @pytest.mark.asyncio
    async def test_send_command(self, mock_server):
        """Verify sending command and receiving response."""
        from sunny.host.transport import AbletonConnection, TransportConfig

        config = TransportConfig(
            host="127.0.0.1",
            tcp_port=mock_server["port"],
            timeout_seconds=1.0,
        )
        conn = AbletonConnection(config)

        try:
            await conn.connect()

            result = await conn.send_command("get_tempo", {"track": 0})

            assert "error" not in result
            assert "result" in result
        finally:
            await conn.disconnect()

    @pytest.mark.asyncio
    async def test_custom_response(self, mock_server):
        """Verify server can return custom responses."""
        from sunny.host.transport import AbletonConnection, TransportConfig

        # Set up custom response
        mock_server["responses"]["get_tempo"] = {
            "id": 1,
            "result": {"tempo": 120.0},
        }

        config = TransportConfig(
            host="127.0.0.1",
            tcp_port=mock_server["port"],
            timeout_seconds=1.0,
        )
        conn = AbletonConnection(config)

        try:
            await conn.connect()

            result = await conn.send_command("get_tempo")

            assert result.get("result", {}).get("tempo") == 120.0
        finally:
            await conn.disconnect()
