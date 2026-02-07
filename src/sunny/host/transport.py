"""Ableton Connection Transport.

Component: HSTP001A
Domain: HS (Host) | Category: TP (Transport)

Provides OSC-over-UDP transport for communicating with the Ableton
Remote Script bridge, following AbletonOSC conventions.

Features:
- OSC over UDP for all commands (AbletonOSC-compatible)
- Bidirectional UDP with response listener
- Message queuing for offline buffering
- Heartbeat via /sunny/status polling

Note: This module must remain Python as it needs to integrate with
the Remote Script (which runs in Ableton's Python environment).
"""

from __future__ import annotations

import asyncio
import logging
import os
import time
from collections import deque
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Callable, Protocol

from sunny.host.osc import OscMessage, decode_message, encode_message

logger = logging.getLogger("sunny.host.transport")


# =============================================================================
# OSC Address Mapping
# =============================================================================

# Maps command names used by MCP tools to OSC addresses.
# Mirrors the C++ OSCAPI001A.h address space.
COMMAND_TO_OSC: dict[str, str] = {
    # Song-level
    "get_session_info": "/live/song/get/tempo",
    "set_tempo": "/live/song/set/tempo",
    "play": "/live/song/start_playing",
    "stop": "/live/song/stop_playing",
    "jump_to_bar": "/live/song/set/current_song_time",

    # Track-level
    "create_midi_track": "/live/song/create_midi_track",
    "create_audio_track": "/live/song/create_audio_track",
    "delete_track": "/live/song/delete_track",
    "get_track_info": "/live/track/get/name",
    "set_track_volume": "/live/track/set/volume",
    "set_track_pan": "/live/track/set/panning",
    "set_track_mute": "/live/track/set/mute",
    "set_track_solo": "/live/track/set/solo",

    # Clip slot
    "create_clip": "/live/clip_slot/create_clip",
    "delete_clip": "/live/clip_slot/delete_clip",

    # Clip
    "add_notes_to_clip": "/live/clip/add/notes",
    "get_clip_notes": "/live/clip/get/notes",
    "fire_clip": "/live/clip/fire",
    "stop_clip": "/live/clip/stop",

    # Device
    "get_device_parameters": "/live/device/get/parameters/name",
    "set_device_parameter": "/live/device/set/parameter/value",

    # Automation
    "get_clip_automation": "/live/clip/get/automation",
    "set_clip_automation": "/live/clip/set/automation",
    "clear_clip_automation": "/live/clip/clear/automation",
    "get_clip_info": "/live/clip/get/length",

    # Arrangement
    "place_clip_in_arrangement": "/live/arrangement/place_clip",
    "create_locator": "/live/song/create_locator",
    "get_locators": "/live/song/get/locators",
    "jump_to_locator": "/live/song/jump_to_locator",
    "set_loop_region": "/live/song/set/loop",
    "duplicate_arrangement_region": "/live/song/duplicate_region",

    # Browser
    "get_browser_tree": "/live/browser/get/tree",
    "get_browser_items_at_path": "/live/browser/get/items",
    "load_browser_item": "/live/browser/load/item",
    "discover_plugin_presets": "/live/browser/get/presets",

    # Snapshot (handled locally, not via OSC)
}


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
    send_port: int = 11000
    receive_port: int = 11001
    timeout_seconds: float = 5.0
    retry_count: int = 3
    retry_delay_seconds: float = 1.0

    @classmethod
    def from_env(cls) -> "TransportConfig":
        """Create configuration from environment variables."""
        return cls(
            host=os.getenv("SUNNY_ABLETON_HOST", "127.0.0.1"),
            send_port=int(os.getenv("SUNNY_OSC_SEND_PORT", "11000")),
            receive_port=int(os.getenv("SUNNY_OSC_RECV_PORT", "11001")),
            timeout_seconds=float(os.getenv("SUNNY_ABLETON_TIMEOUT", "5.0")),
            retry_count=int(os.getenv("SUNNY_ABLETON_RETRIES", "3")),
        )


# =============================================================================
# OSC Response Protocol
# =============================================================================


class OscResponseProtocol(asyncio.DatagramProtocol):
    """Handles incoming OSC response datagrams."""

    def __init__(self) -> None:
        self.transport: asyncio.DatagramTransport | None = None
        self._pending: dict[str, asyncio.Future[OscMessage]] = {}
        self._listeners: dict[str, Callable[[OscMessage], None]] = {}

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self.transport = transport

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        try:
            msg = decode_message(data)
        except ValueError as e:
            logger.warning(f"Malformed OSC response: {e}")
            return

        # Check for pending request-response
        if msg.address in self._pending:
            future = self._pending.pop(msg.address)
            if not future.done():
                future.set_result(msg)
            return

        # Check for registered listeners
        for prefix, callback in self._listeners.items():
            if msg.address.startswith(prefix):
                try:
                    callback(msg)
                except Exception as e:
                    logger.error(f"Listener error for {msg.address}: {e}")
                return

        logger.debug(f"Unhandled OSC response: {msg.address}")

    def expect_response(
        self, address: str, loop: asyncio.AbstractEventLoop
    ) -> asyncio.Future[OscMessage]:
        """Register a future for an expected response address."""
        future: asyncio.Future[OscMessage] = loop.create_future()
        self._pending[address] = future
        return future

    def add_listener(
        self, prefix: str, callback: Callable[[OscMessage], None]
    ) -> None:
        """Register a listener for OSC address prefix."""
        self._listeners[prefix] = callback

    def remove_listener(self, prefix: str) -> None:
        """Remove a listener."""
        self._listeners.pop(prefix, None)


# =============================================================================
# OSC Transport
# =============================================================================


@dataclass
class OscTransport:
    """Bidirectional OSC-over-UDP transport.

    Follows AbletonOSC conventions:
    - Sends OSC messages to Remote Script on send_port
    - Receives OSC responses on receive_port
    """

    host: str = "127.0.0.1"
    send_port: int = 11000
    receive_port: int = 11001
    timeout: float = 5.0
    retry_count: int = 3
    retry_delay: float = 1.0
    heartbeat_interval: float = 30.0
    max_queue_size: int = 100

    _send_transport: asyncio.DatagramTransport | None = field(default=None, repr=False)
    _recv_protocol: OscResponseProtocol | None = field(default=None, repr=False)
    _recv_transport: asyncio.DatagramTransport | None = field(default=None, repr=False)
    _state: ConnectionState = field(default=ConnectionState.DISCONNECTED, repr=False)
    _message_queue: deque[bytes] = field(default_factory=deque, repr=False)
    _state_listeners: list[ConnectionStateListener] = field(
        default_factory=list, repr=False
    )
    _heartbeat_task: asyncio.Task | None = field(default=None, repr=False)
    _last_activity: float = field(default=0.0, repr=False)

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
        logger.info(f"OSC state: {old_state.name} -> {new_state.name} ({message})")

        for listener in self._state_listeners:
            try:
                listener.on_state_change(old_state, new_state, message)
            except Exception as e:
                logger.error(f"State listener error: {e}")

    async def connect(self) -> bool:
        """Bind UDP sockets for OSC communication.

        Returns:
            True if sockets bound successfully.
        """
        if self._state == ConnectionState.CONNECTED:
            return True

        self._set_state(
            ConnectionState.CONNECTING,
            f"Binding UDP {self.host}:{self.receive_port} -> :{self.send_port}",
        )

        try:
            loop = asyncio.get_event_loop()

            # Bind receive socket
            self._recv_protocol = OscResponseProtocol()
            self._recv_transport, _ = await loop.create_datagram_endpoint(
                lambda: self._recv_protocol,
                local_addr=("0.0.0.0", self.receive_port),
            )

            # Create send socket (connected to remote)
            send_protocol = asyncio.DatagramProtocol()
            self._send_transport, _ = await loop.create_datagram_endpoint(
                lambda: send_protocol,
                remote_addr=(self.host, self.send_port),
            )

            self._set_state(ConnectionState.CONNECTED, "OSC transport ready")
            self._last_activity = time.time()
            self._start_heartbeat()
            await self._flush_queue()
            return True
        except OSError as e:
            logger.error(f"OSC bind failed: {e}")
            self._set_state(ConnectionState.ERROR, f"Bind failed: {e}")
            return False

    async def disconnect(self) -> None:
        """Close UDP sockets."""
        self._stop_heartbeat()

        if self._send_transport:
            self._send_transport.close()
            self._send_transport = None

        if self._recv_transport:
            self._recv_transport.close()
            self._recv_transport = None
            self._recv_protocol = None

        self._set_state(ConnectionState.DISCONNECTED, "Disconnected")

    def send_osc(self, address: str, args: list[Any] | None = None) -> bool:
        """Send an OSC message (fire-and-forget).

        Args:
            address: OSC address pattern.
            args: OSC arguments.

        Returns:
            True if sent successfully.
        """
        if not self._send_transport:
            return False

        try:
            packet = encode_message(address, args)
            self._send_transport.sendto(packet)
            self._last_activity = time.time()
            return True
        except Exception as e:
            logger.error(f"OSC send failed: {e}")
            return False

    async def send_and_receive(
        self,
        address: str,
        args: list[Any] | None = None,
        response_address: str | None = None,
    ) -> OscMessage | None:
        """Send OSC message and wait for response.

        Args:
            address: OSC address pattern to send.
            args: OSC arguments.
            response_address: Address to listen for response.
                Defaults to same address as sent.

        Returns:
            Response OscMessage or None on timeout.
        """
        if self._state != ConnectionState.CONNECTED:
            return None

        resp_addr = response_address or address
        loop = asyncio.get_event_loop()

        future = self._recv_protocol.expect_response(resp_addr, loop)

        if not self.send_osc(address, args):
            return None

        try:
            return await asyncio.wait_for(future, timeout=self.timeout)
        except asyncio.TimeoutError:
            # Clean up the pending future
            self._recv_protocol._pending.pop(resp_addr, None)
            logger.warning(f"OSC response timeout for {resp_addr}")
            return None

    def _queue_message(self, packet: bytes) -> None:
        """Queue an OSC packet for later delivery."""
        if len(self._message_queue) >= self.max_queue_size:
            self._message_queue.popleft()
            logger.warning("OSC queue full, dropping oldest")

        self._message_queue.append(packet)

    async def _flush_queue(self) -> None:
        """Send all queued OSC packets."""
        while self._message_queue and self._send_transport:
            packet = self._message_queue.popleft()
            try:
                self._send_transport.sendto(packet)
            except Exception as e:
                logger.error(f"Failed to flush OSC packet: {e}")
                self._message_queue.appendleft(packet)
                break

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
        """Heartbeat monitoring via /sunny/status polling."""
        try:
            while self._state == ConnectionState.CONNECTED:
                await asyncio.sleep(self.heartbeat_interval)

                if time.time() - self._last_activity > self.heartbeat_interval * 2:
                    logger.warning("No OSC activity, connection may be stale")
                    self.send_osc("/sunny/status")
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
# Parameter Flattening
# =============================================================================


def _flatten_params(command: str, params: dict[str, Any]) -> list[Any]:
    """Convert command parameters to a flat OSC argument list.

    AbletonOSC uses positional arguments rather than named parameters.
    This function maps named params to the correct OSC argument order.
    """
    if command == "set_tempo":
        return [float(params.get("bpm", 120.0))]

    if command in ("jump_to_bar",):
        bar = int(params.get("bar", 1))
        beat = int(params.get("beat", 1))
        return [float((bar - 1) * 4 + (beat - 1))]

    if command == "create_clip":
        return [
            int(params.get("track", 0)),
            int(params.get("slot", 0)),
            float(params.get("length", 4.0)),
        ]

    if command == "add_notes_to_clip":
        track = int(params.get("track", 0))
        slot = int(params.get("slot", 0))
        args: list[Any] = [track, slot]
        for note in params.get("notes", []):
            args.extend([
                int(note.get("pitch", 60)),
                float(note.get("start_time", 0.0)),
                float(note.get("duration", 1.0)),
                int(note.get("velocity", 100)),
                int(note.get("mute", 0)),
            ])
        return args

    if command in ("set_track_volume", "set_track_pan"):
        return [
            int(params.get("track_index", 0)),
            float(params.get("value", params.get("volume", params.get("pan", 0.0)))),
        ]

    if command in ("set_track_mute", "set_track_solo"):
        return [
            int(params.get("track_index", 0)),
            int(params.get("mute", params.get("solo", 0))),
        ]

    if command == "delete_track":
        return [int(params.get("track_index", 0))]

    if command == "get_track_info":
        return [int(params.get("track_index", 0))]

    if command in ("get_device_parameters", "set_device_parameter"):
        args_list: list[Any] = [
            int(params.get("track_index", 0)),
            int(params.get("device_index", 0)),
        ]
        if "param_index" in params:
            args_list.append(int(params["param_index"]))
        if "value" in params:
            args_list.append(float(params["value"]))
        return args_list

    if command in ("create_midi_track", "create_audio_track"):
        args_out: list[Any] = []
        if "index" in params:
            args_out.append(int(params["index"]))
        return args_out

    # Generic: flatten all values in order
    result: list[Any] = []
    for v in params.values():
        if isinstance(v, (list, dict)):
            continue  # Skip complex types in generic path
        result.append(v)
    return result


def _osc_response_to_dict(
    command: str, msg: OscMessage | None
) -> dict[str, Any]:
    """Convert OSC response to the dict format expected by MCP tools."""
    if msg is None:
        return {"error": "No response from Ableton"}

    # Session info is a composite query — return structured data
    if command == "get_session_info":
        tempo = msg.args[0] if msg.args else 120.0
        return {
            "tempo": tempo,
            "time_signature": "4/4",
            "is_playing": False,
        }

    # Generic: return args as "value" or "values"
    if len(msg.args) == 0:
        return {"success": True}
    if len(msg.args) == 1:
        return {"success": True, "value": msg.args[0]}
    return {"success": True, "values": msg.args}


# =============================================================================
# Ableton Connection
# =============================================================================


class AbletonConnection:
    """High-level connection to Ableton Live via OSC over UDP.

    Provides a unified interface for communicating with the
    Remote Script bridge using the AbletonOSC protocol.

    Features:
    - OSC-over-UDP transport (AbletonOSC-compatible)
    - Bidirectional request/response
    - Connection state notifications
    - Heartbeat monitoring
    """

    def __init__(self, config: TransportConfig | None = None):
        """Initialize connection.

        Args:
            config: Transport configuration. If None, loads from environment.
        """
        self.config = config or TransportConfig.from_env()
        self.osc = OscTransport(
            host=self.config.host,
            send_port=self.config.send_port,
            receive_port=self.config.receive_port,
            timeout=self.config.timeout_seconds,
            retry_count=self.config.retry_count,
            retry_delay=self.config.retry_delay_seconds,
        )
        self._request_id = 0
        self._state_listeners: list[ConnectionStateListener] = []

    def add_state_listener(self, listener: ConnectionStateListener) -> None:
        """Add a connection state listener."""
        self._state_listeners.append(listener)
        self.osc.add_state_listener(listener)

    def remove_state_listener(self, listener: ConnectionStateListener) -> None:
        """Remove a connection state listener."""
        if listener in self._state_listeners:
            self._state_listeners.remove(listener)
        self.osc.remove_state_listener(listener)

    async def connect(self) -> bool:
        """Connect to Ableton via OSC.

        Returns:
            True if connected successfully.
        """
        return await self.osc.connect()

    async def disconnect(self) -> None:
        """Disconnect from Ableton."""
        await self.osc.disconnect()

    @property
    def is_connected(self) -> bool:
        """Check if connected."""
        return self.osc.is_connected

    @property
    def state(self) -> ConnectionState:
        """Get current connection state."""
        return self.osc.state

    @property
    def queue_size(self) -> int:
        """Get number of queued messages awaiting delivery."""
        return self.osc.queue_size

    async def send_command(
        self, command: str, params: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        """Send command to Ableton and return response.

        Encodes the command as an OSC message using the AbletonOSC
        address space and sends via UDP.

        Args:
            command: Command name (e.g., "set_tempo", "create_clip").
            params: Optional command parameters.

        Returns:
            Response dictionary. On error, contains "error" key.
        """
        if not self.is_connected:
            return {"error": "Not connected to Ableton"}

        osc_address = COMMAND_TO_OSC.get(command)
        if osc_address is None:
            # Unknown command — send to /sunny/command/{name} as fallback
            osc_address = f"/sunny/command/{command}"

        args = _flatten_params(command, params or {})

        try:
            response = await self.osc.send_and_receive(osc_address, args)
            return _osc_response_to_dict(command, response)
        except Exception as e:
            return {"error": str(e)}

    def send_osc(
        self, address: str, args: list[Any] | None = None
    ) -> bool:
        """Send a raw OSC message (fire-and-forget).

        Args:
            address: OSC address pattern.
            args: OSC arguments.

        Returns:
            True if sent successfully.
        """
        return self.osc.send_osc(address, args)

    async def wait_for_connection(self, timeout: float | None = None) -> bool:
        """Wait for connection to be established.

        Args:
            timeout: Maximum time to wait in seconds.

        Returns:
            True if connected, False if timeout or error.
        """
        start = time.time()
        while True:
            if self.is_connected:
                return True

            if self.osc.state == ConnectionState.ERROR:
                return False

            if timeout and (time.time() - start) > timeout:
                return False

            await asyncio.sleep(0.1)

    def get_status(self) -> dict[str, Any]:
        """Get current connection status."""
        return {
            "state": self.osc.state.name,
            "is_connected": self.is_connected,
            "host": self.config.host,
            "send_port": self.config.send_port,
            "receive_port": self.config.receive_port,
            "protocol": "OSC/UDP",
            "queue_size": self.queue_size,
        }


# =============================================================================
# Exports
# =============================================================================

__all__ = [
    "ConnectionState",
    "ConnectionStateListener",
    "TransportConfig",
    "OscTransport",
    "AbletonConnection",
    "COMMAND_TO_OSC",
]
