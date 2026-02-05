"""Ableton Connection Transport.

Component: HSTP001A
Domain: HS (Host) | Category: TP (Transport)

Provides TCP/UDP transport for communicating with the Ableton
Remote Script bridge.

Features:
- TCP for reliable command/response with auto-reconnection
- UDP for low-latency events (optional)
- Message queuing for offline buffering
- Heartbeat/keepalive for connection monitoring

Note: This module must remain Python as it needs to integrate with
the Remote Script (which runs in Ableton's Python environment).
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import time
from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Callable, Protocol, runtime_checkable

logger = logging.getLogger("sunny.host.transport")


# =============================================================================
# Connection State
# =============================================================================


class ConnectionState(Enum):
    """Transport connection state."""

    DISCONNECTED = auto()
    CONNECTING = auto()
    CONNECTED = auto()
    RECONNECTING = auto()
    ERROR = auto()


class ConnectionStateListener(Protocol):
    """Protocol for connection state change notifications."""

    def on_state_change(
        self, old_state: ConnectionState, new_state: ConnectionState, message: str
    ) -> None:
        """Called when connection state changes."""
        ...


# =============================================================================
# Configuration
# =============================================================================


@dataclass
class TransportConfig:
    """Transport connection configuration."""

    host: str = "127.0.0.1"
    tcp_port: int = 9001
    udp_port: int = 9002
    timeout_seconds: float = 5.0
    retry_count: int = 3
    retry_delay_seconds: float = 1.0

    @classmethod
    def from_env(cls) -> "TransportConfig":
        """Create configuration from environment variables."""
        return cls(
            host=os.getenv("SUNNY_ABLETON_HOST", "127.0.0.1"),
            tcp_port=int(os.getenv("SUNNY_ABLETON_TCP_PORT", "9001")),
            udp_port=int(os.getenv("SUNNY_ABLETON_UDP_PORT", "9002")),
            timeout_seconds=float(os.getenv("SUNNY_ABLETON_TIMEOUT", "5.0")),
            retry_count=int(os.getenv("SUNNY_ABLETON_RETRIES", "3")),
        )


# =============================================================================
# TCP Transport
# =============================================================================


@dataclass
class TcpTransport:
    """TCP transport for reliable command/response.

    This handles the TCP connection to the Remote Script bridge
    for commands that need guaranteed delivery and response.

    Features:
    - Auto-reconnection on disconnect
    - Message queuing during reconnection
    - Heartbeat monitoring
    """

    host: str = "127.0.0.1"
    port: int = 9001
    timeout: float = 5.0
    retry_count: int = 3
    retry_delay: float = 1.0
    heartbeat_interval: float = 30.0
    max_queue_size: int = 100

    _reader: asyncio.StreamReader | None = field(default=None, repr=False)
    _writer: asyncio.StreamWriter | None = field(default=None, repr=False)
    _state: ConnectionState = field(default=ConnectionState.DISCONNECTED, repr=False)
    _message_queue: deque = field(default_factory=deque, repr=False)
    _state_listeners: list = field(default_factory=list, repr=False)
    _heartbeat_task: asyncio.Task | None = field(default=None, repr=False)
    _last_activity: float = field(default=0.0, repr=False)
    _reconnect_task: asyncio.Task | None = field(default=None, repr=False)

    def add_state_listener(self, listener: ConnectionStateListener) -> None:
        """Add a connection state listener."""
        self._state_listeners.append(listener)

    def remove_state_listener(self, listener: ConnectionStateListener) -> None:
        """Remove a connection state listener."""
        if listener in self._state_listeners:
            self._state_listeners.remove(listener)

    def _set_state(self, new_state: ConnectionState, message: str = "") -> None:
        """Update connection state and notify listeners."""
        if self._state == new_state:
            return

        old_state = self._state
        self._state = new_state
        logger.info(f"TCP state: {old_state.name} -> {new_state.name} ({message})")

        for listener in self._state_listeners:
            try:
                listener.on_state_change(old_state, new_state, message)
            except Exception as e:
                logger.error(f"State listener error: {e}")

    async def connect(self) -> bool:
        """Establish TCP connection with retry.

        Returns:
            True if connected successfully.
        """
        if self._state == ConnectionState.CONNECTED:
            return True

        self._set_state(ConnectionState.CONNECTING, f"Connecting to {self.host}:{self.port}")

        for attempt in range(self.retry_count):
            try:
                self._reader, self._writer = await asyncio.wait_for(
                    asyncio.open_connection(self.host, self.port),
                    timeout=self.timeout,
                )
                self._set_state(ConnectionState.CONNECTED, "Connected")
                self._last_activity = time.time()
                self._start_heartbeat()
                await self._flush_queue()
                return True
            except (asyncio.TimeoutError, OSError) as e:
                logger.warning(f"TCP connection attempt {attempt + 1}/{self.retry_count} failed: {e}")
                if attempt < self.retry_count - 1:
                    await asyncio.sleep(self.retry_delay)

        self._set_state(ConnectionState.ERROR, "Connection failed after retries")
        return False

    async def disconnect(self) -> None:
        """Close TCP connection."""
        self._stop_heartbeat()

        if self._reconnect_task:
            self._reconnect_task.cancel()
            self._reconnect_task = None

        if self._writer:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception as e:
                logger.debug(f"Error closing TCP connection: {e}")
            finally:
                self._writer = None
                self._reader = None

        self._set_state(ConnectionState.DISCONNECTED, "Disconnected")

    async def send(self, message: str) -> str | None:
        """Send message and receive response.

        If disconnected, queues the message for later delivery.

        Args:
            message: JSON message to send.

        Returns:
            Response string or None if failed.
        """
        if self._state != ConnectionState.CONNECTED:
            if self._state == ConnectionState.RECONNECTING:
                self._queue_message(message)
                return None
            logger.error("TCP not connected")
            return None

        try:
            # Send message with newline delimiter
            self._writer.write((message + "\n").encode("utf-8"))
            await self._writer.drain()

            # Read response
            response = await asyncio.wait_for(
                self._reader.readline(),
                timeout=self.timeout,
            )
            self._last_activity = time.time()
            return response.decode("utf-8").strip()
        except (asyncio.TimeoutError, OSError) as e:
            logger.error(f"TCP send/receive failed: {e}")
            self._queue_message(message)
            await self._start_reconnect()
            return None

    def _queue_message(self, message: str) -> None:
        """Queue message for later delivery."""
        if len(self._message_queue) >= self.max_queue_size:
            self._message_queue.popleft()  # Drop oldest
            logger.warning("Message queue full, dropping oldest message")

        self._message_queue.append(message)
        logger.debug(f"Queued message, queue size: {len(self._message_queue)}")

    async def _flush_queue(self) -> None:
        """Send all queued messages."""
        while self._message_queue and self._state == ConnectionState.CONNECTED:
            message = self._message_queue.popleft()
            try:
                self._writer.write((message + "\n").encode("utf-8"))
                await self._writer.drain()
                logger.debug(f"Flushed queued message, {len(self._message_queue)} remaining")
            except Exception as e:
                logger.error(f"Failed to flush message: {e}")
                self._message_queue.appendleft(message)  # Re-queue
                break

    async def _start_reconnect(self) -> None:
        """Start background reconnection."""
        if self._reconnect_task and not self._reconnect_task.done():
            return  # Already reconnecting

        self._set_state(ConnectionState.RECONNECTING, "Connection lost, reconnecting")
        self._stop_heartbeat()

        if self._writer:
            try:
                self._writer.close()
            except Exception:
                pass
            self._writer = None
            self._reader = None

        self._reconnect_task = asyncio.create_task(self._reconnect_loop())

    async def _reconnect_loop(self) -> None:
        """Background reconnection loop."""
        while self._state == ConnectionState.RECONNECTING:
            await asyncio.sleep(self.retry_delay)

            try:
                self._reader, self._writer = await asyncio.wait_for(
                    asyncio.open_connection(self.host, self.port),
                    timeout=self.timeout,
                )
                self._set_state(ConnectionState.CONNECTED, "Reconnected")
                self._last_activity = time.time()
                self._start_heartbeat()
                await self._flush_queue()
                return
            except (asyncio.TimeoutError, OSError) as e:
                logger.debug(f"Reconnection attempt failed: {e}")

    def _start_heartbeat(self) -> None:
        """Start heartbeat monitoring."""
        self._stop_heartbeat()
        if self.heartbeat_interval > 0:
            self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())

    def _stop_heartbeat(self) -> None:
        """Stop heartbeat monitoring."""
        if self._heartbeat_task:
            self._heartbeat_task.cancel()
            self._heartbeat_task = None

    async def _heartbeat_loop(self) -> None:
        """Heartbeat monitoring loop."""
        try:
            while self._state == ConnectionState.CONNECTED:
                await asyncio.sleep(self.heartbeat_interval)

                # Check for activity timeout
                if time.time() - self._last_activity > self.heartbeat_interval * 2:
                    logger.warning("No activity, connection may be stale")
                    # Could send a ping here if protocol supports it
        except asyncio.CancelledError:
            pass

    @property
    def is_connected(self) -> bool:
        """Check if connected."""
        return self._state == ConnectionState.CONNECTED

    @property
    def state(self) -> ConnectionState:
        """Get current connection state."""
        return self._state

    @property
    def queue_size(self) -> int:
        """Get number of queued messages."""
        return len(self._message_queue)


# =============================================================================
# UDP Transport
# =============================================================================


class UdpProtocol(asyncio.DatagramProtocol):
    """UDP protocol handler for low-latency events."""

    def __init__(self, message_callback: Callable[[bytes], None] | None = None):
        self.message_callback = message_callback
        self.transport: asyncio.DatagramTransport | None = None

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self.transport = transport

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        if self.message_callback:
            self.message_callback(data)


@dataclass
class UdpTransport:
    """UDP transport for low-latency events.

    This handles UDP for events where low latency is more
    important than guaranteed delivery.
    """

    host: str = "127.0.0.1"
    port: int = 9002

    _transport: asyncio.DatagramTransport | None = field(default=None, repr=False)
    _protocol: UdpProtocol | None = field(default=None, repr=False)
    _connected: bool = field(default=False, repr=False)

    async def connect(
        self, message_callback: Callable[[bytes], None] | None = None
    ) -> bool:
        """Start UDP transport.

        Args:
            message_callback: Optional callback for received messages.

        Returns:
            True if started successfully.
        """
        if self._connected:
            return True

        try:
            loop = asyncio.get_event_loop()
            self._transport, self._protocol = await loop.create_datagram_endpoint(
                lambda: UdpProtocol(message_callback),
                remote_addr=(self.host, self.port),
            )
            self._connected = True
            logger.info(f"UDP connected to {self.host}:{self.port}")
            return True
        except OSError as e:
            logger.warning(f"UDP connection failed: {e}")
            return False

    async def disconnect(self) -> None:
        """Stop UDP transport."""
        if self._transport:
            self._transport.close()
            self._transport = None
            self._protocol = None
            self._connected = False

    def send(self, message: bytes) -> bool:
        """Send UDP datagram.

        Args:
            message: Message bytes to send.

        Returns:
            True if sent successfully.
        """
        if not self._connected or not self._transport:
            return False

        try:
            self._transport.sendto(message)
            return True
        except Exception as e:
            logger.error(f"UDP send failed: {e}")
            return False

    @property
    def is_connected(self) -> bool:
        """Check if connected."""
        return self._connected


# =============================================================================
# Ableton Connection
# =============================================================================


class AbletonConnection:
    """High-level connection to Ableton Live.

    Provides a unified interface for communicating with the
    Remote Script bridge, handling both TCP (reliable) and
    UDP (low-latency) transports.

    Features:
    - Auto-reconnection with message queuing
    - Heartbeat monitoring
    - Connection state notifications

    This class implements the AbletonTransport protocol expected
    by SnapshotManager and other components.
    """

    def __init__(self, config: TransportConfig | None = None):
        """Initialize connection.

        Args:
            config: Transport configuration. If None, loads from environment.
        """
        self.config = config or TransportConfig.from_env()
        self.tcp = TcpTransport(
            host=self.config.host,
            port=self.config.tcp_port,
            timeout=self.config.timeout_seconds,
            retry_count=self.config.retry_count,
            retry_delay=self.config.retry_delay_seconds,
        )
        self.udp = UdpTransport(
            host=self.config.host,
            port=self.config.udp_port,
        )
        self._request_id = 0
        self._state_listeners: list[ConnectionStateListener] = []

    def add_state_listener(self, listener: ConnectionStateListener) -> None:
        """Add a connection state listener.

        Args:
            listener: Listener to receive state change notifications.
        """
        self._state_listeners.append(listener)
        self.tcp.add_state_listener(listener)

    def remove_state_listener(self, listener: ConnectionStateListener) -> None:
        """Remove a connection state listener.

        Args:
            listener: Listener to remove.
        """
        if listener in self._state_listeners:
            self._state_listeners.remove(listener)
        self.tcp.remove_state_listener(listener)

    async def connect(self) -> bool:
        """Connect to Ableton.

        Returns:
            True if connected successfully.
        """
        # Try TCP first (required)
        tcp_ok = await self.tcp.connect()
        if not tcp_ok:
            return False

        # UDP is optional
        await self.udp.connect()

        return True

    async def disconnect(self) -> None:
        """Disconnect from Ableton."""
        await self.tcp.disconnect()
        await self.udp.disconnect()

    @property
    def is_connected(self) -> bool:
        """Check if connected."""
        return self.tcp.is_connected

    @property
    def state(self) -> ConnectionState:
        """Get current connection state."""
        return self.tcp.state

    @property
    def queue_size(self) -> int:
        """Get number of queued messages awaiting reconnection."""
        return self.tcp.queue_size

    async def send_command(
        self, command: str, params: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Send command to Ableton and return response.

        This is the main interface for MCP tools to interact with
        Ableton Live via the Remote Script bridge.

        If the connection is lost, the message is queued and will be
        sent when reconnected. In this case, returns immediately with
        a "queued" status.

        Args:
            command: Command name (e.g., "get_tempo", "set_clip_notes").
            params: Optional command parameters.

        Returns:
            Response dictionary. On error, contains "error" key.
            If queued during reconnection, contains "queued" key.
        """
        if self.tcp.state == ConnectionState.RECONNECTING:
            # Queue the message and return immediately
            self._request_id += 1
            request = {
                "id": self._request_id,
                "command": command,
                "params": params or {},
            }
            self.tcp._queue_message(json.dumps(request))
            return {
                "queued": True,
                "id": self._request_id,
                "message": "Command queued, will be sent when reconnected",
            }

        if not self.is_connected:
            return {"error": "Not connected to Ableton"}

        # Build request
        self._request_id += 1
        request = {
            "id": self._request_id,
            "command": command,
            "params": params or {},
        }

        # Send via TCP
        try:
            message = json.dumps(request)
            response_str = await self.tcp.send(message)

            if response_str is None:
                # Message was queued during reconnection attempt
                return {
                    "queued": True,
                    "id": self._request_id,
                    "message": "Command queued, will be sent when reconnected",
                }

            response = json.loads(response_str)
            return response
        except json.JSONDecodeError as e:
            return {"error": f"Invalid response: {e}"}
        except Exception as e:
            return {"error": str(e)}

    async def send_udp_event(self, event_type: str, data: dict[str, Any]) -> bool:
        """Send low-latency event via UDP.

        Args:
            event_type: Event type identifier.
            data: Event data.

        Returns:
            True if sent successfully.
        """
        if not self.udp.is_connected:
            return False

        try:
            message = json.dumps({"type": event_type, "data": data}).encode("utf-8")
            return self.udp.send(message)
        except Exception as e:
            logger.error(f"UDP event send failed: {e}")
            return False

    async def wait_for_connection(self, timeout: float | None = None) -> bool:
        """Wait for connection to be established.

        Args:
            timeout: Maximum time to wait in seconds. None for no timeout.

        Returns:
            True if connected, False if timeout or error.
        """
        start = time.time()
        while True:
            if self.is_connected:
                return True

            if self.tcp.state == ConnectionState.ERROR:
                return False

            if timeout and (time.time() - start) > timeout:
                return False

            await asyncio.sleep(0.1)

    def get_status(self) -> dict[str, Any]:
        """Get current connection status.

        Returns:
            Dictionary with connection status information.
        """
        return {
            "state": self.tcp.state.name,
            "is_connected": self.is_connected,
            "host": self.config.host,
            "tcp_port": self.config.tcp_port,
            "udp_port": self.config.udp_port,
            "udp_connected": self.udp.is_connected,
            "queue_size": self.queue_size,
        }


# =============================================================================
# Exports
# =============================================================================

__all__ = [
    "ConnectionState",
    "ConnectionStateListener",
    "TransportConfig",
    "TcpTransport",
    "UdpTransport",
    "AbletonConnection",
]
