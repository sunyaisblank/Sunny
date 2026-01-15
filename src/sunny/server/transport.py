"""Sunny - Transport Layer.

Component: SRTR001A
Domain: SR (Server) | Category: TR (Transport)

Provides hybrid TCP/UDP communication with Ableton Live.
- TCP: Reliable commands (session queries, track creation)
- UDP/OSC: Low-latency parameter modulation

Bounds:
    - host: str (valid IPv4/hostname)
    - port: int ∈ [1024, 65535]
    - timeout: float ∈ [0.5, 60.0] seconds

Error Codes:
    - 1100: Connection failed
    - 1101: Connection timeout
    - 1102: Invalid response format

Enterprise Features:
    - Automatic reconnection with exponential backoff
    - Connection health monitoring
    - Graceful degradation to offline mode
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import socket
import time
from dataclasses import dataclass, field
from typing import Any

logger = logging.getLogger("sunny.transport")


# =============================================================================
# Connection Resilience
# =============================================================================


@dataclass
class RetryConfig:
    """Configuration for connection retry behavior."""

    max_retries: int = 3
    base_delay: float = 1.0  # Initial delay in seconds
    max_delay: float = 30.0  # Maximum delay between retries
    exponential_base: float = 2.0  # Backoff multiplier


@dataclass
class ConnectionHealth:
    """Tracks connection health metrics."""

    connected: bool = False
    last_success: float = 0.0
    last_failure: float = 0.0
    consecutive_failures: int = 0
    total_reconnects: int = 0

    def record_success(self) -> None:
        """Record a successful operation."""
        self.connected = True
        self.last_success = time.time()
        self.consecutive_failures = 0

    def record_failure(self) -> None:
        """Record a failed operation."""
        self.last_failure = time.time()
        self.consecutive_failures += 1

    def record_reconnect(self) -> None:
        """Record a reconnection attempt."""
        self.total_reconnects += 1


async def retry_with_backoff(
    operation,
    config: RetryConfig,
    operation_name: str = "operation",
) -> Any:
    """Execute an operation with exponential backoff retry.

    Args:
        operation: Async callable to execute.
        config: Retry configuration.
        operation_name: Name for logging.

    Returns:
        Result from the operation.

    Raises:
        The last exception if all retries fail.
    """
    last_exception = None
    delay = config.base_delay

    for attempt in range(config.max_retries + 1):
        try:
            return await operation()
        except (ConnectionError, TimeoutError, socket.error) as e:
            last_exception = e
            if attempt < config.max_retries:
                logger.warning(
                    f"{operation_name} failed (attempt {attempt + 1}/{config.max_retries + 1}): {e}"
                )
                logger.info(f"Retrying in {delay:.1f}s...")
                await asyncio.sleep(delay)
                delay = min(delay * config.exponential_base, config.max_delay)
            else:
                logger.error(f"{operation_name} failed after {config.max_retries + 1} attempts")

    raise last_exception or ConnectionError(f"{operation_name} failed")


class TCPConnection:
    """TCP socket connection for reliable Ableton commands.
    
    Component: SRTR001A
    
    Bounds:
        - host: str (valid IPv4 address or hostname)
        - port: int ∈ [1024, 65535]
        - timeout: float ∈ [0.5, 60.0]
    """
    
    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int | None = None,
        timeout: float = 5.0
    ):
        """Initialize TCP connection.
        
        Args:
            host: Ableton host address
            port: TCP port (default from env or 9876)
            timeout: Socket timeout in seconds
        """
        self.host = host
        self.port = port or int(os.getenv("SUNNY_TCP_PORT", "9876"))
        self.timeout = timeout
        self._socket: socket.socket | None = None
        self._lock = asyncio.Lock()
    
    async def connect(self) -> bool:
        """Establish TCP connection to Ableton.
        
        Returns:
            True if connected successfully
        """
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self._socket.settimeout(self.timeout)
            self._socket.connect((self.host, self.port))
            logger.info(f"TCP connected to {self.host}:{self.port}")
            return True
        except (socket.error, ConnectionRefusedError) as e:
            logger.warning(f"TCP connection failed: {e}")
            self._socket = None
            return False
    
    async def disconnect(self):
        """Close TCP connection."""
        if self._socket:
            try:
                self._socket.close()
            except Exception:
                pass
            self._socket = None
            logger.info("TCP disconnected")
    
    async def send(self, command: str, params: dict | None = None) -> dict:
        """Send command and receive response.
        
        Args:
            command: Command type name
            params: Optional command parameters
        
        Returns:
            Response dictionary from Ableton
        
        Raises:
            ConnectionError: If not connected
            TimeoutError: If response times out
        """
        if not self._socket:
            # Attempt to reconnect
            if not await self.connect():
                raise ConnectionError("TCP not connected to Ableton")
        
        async with self._lock:
            message = {
                "type": command,
                "params": params or {},
            }
            
            try:
                # Send message
                data = json.dumps(message).encode("utf-8")
                self._socket.sendall(data)
                logger.debug(f"Sent command: {command}")
                
                # Receive response with chunked reading
                chunks = []
                self._socket.settimeout(30.0)  # Longer timeout for responses
                
                while True:
                    try:
                        chunk = self._socket.recv(8192)
                        if not chunk:
                            if not chunks:
                                raise ConnectionError("Connection closed before response")
                            break
                        
                        chunks.append(chunk)
                        
                        # Try to parse complete JSON
                        try:
                            response_data = b''.join(chunks)
                            response = json.loads(response_data.decode("utf-8"))
                            logger.debug(f"Received response: {len(response_data)} bytes")
                            
                            if response.get("status") == "error":
                                raise RuntimeError(response.get("message", "Unknown error"))
                            
                            return response.get("result", response)
                        except json.JSONDecodeError:
                            # Incomplete JSON, continue receiving
                            continue
                            
                    except socket.timeout:
                        if chunks:
                            # Try to parse what we have
                            try:
                                response_data = b''.join(chunks)
                                response = json.loads(response_data.decode("utf-8"))
                                if response.get("status") == "error":
                                    raise RuntimeError(response.get("message", "Unknown error"))
                                return response.get("result", response)
                            except json.JSONDecodeError:
                                pass
                        raise TimeoutError(f"Timeout waiting for response to {command}")
                
                # If we get here with chunks, try to parse
                if chunks:
                    response_data = b''.join(chunks)
                    response = json.loads(response_data.decode("utf-8"))
                    if response.get("status") == "error":
                        raise RuntimeError(response.get("message", "Unknown error"))
                    return response.get("result", response)
                
                raise RuntimeError("No response received")
                
            except socket.timeout:
                self._socket = None  # Mark for reconnection
                raise TimeoutError(f"Timeout waiting for response to {command}")
            except (ConnectionError, BrokenPipeError, ConnectionResetError) as e:
                logger.warning(f"Connection lost: {e}")
                self._socket = None  # Mark for reconnection
                raise ConnectionError(f"Connection lost: {e}")
            except json.JSONDecodeError as e:
                raise RuntimeError(f"Invalid response from Ableton: {e}")


class UDPConnection:
    """UDP/OSC connection for low-latency parameter control."""
    
    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int | None = None
    ):
        """Initialize UDP connection.
        
        Args:
            host: Ableton host address
            port: UDP port (default from env or 9877)
        """
        self.host = host
        self.port = port or int(os.getenv("SUNNY_UDP_PORT", "9877"))
        self._socket: socket.socket | None = None
    
    async def connect(self) -> bool:
        """Create UDP socket.
        
        Returns:
            True (UDP is connectionless)
        """
        try:
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            logger.info(f"UDP socket ready for {self.host}:{self.port}")
            return True
        except socket.error as e:
            logger.warning(f"UDP socket creation failed: {e}")
            return False
    
    async def disconnect(self):
        """Close UDP socket."""
        if self._socket:
            try:
                self._socket.close()
            except Exception:
                pass
            self._socket = None
    
    def send_osc(self, address: str, *args: Any):
        """Send OSC message via UDP.
        
        Args:
            address: OSC address (e.g., "/track/0/volume")
            *args: OSC arguments
        """
        if not self._socket:
            logger.warning("UDP socket not ready")
            return
        
        try:
            # Import python-osc for OSC message building
            from pythonosc import osc_message_builder
            
            builder = osc_message_builder.OscMessageBuilder(address=address)
            for arg in args:
                builder.add_arg(arg)
            
            msg = builder.build()
            self._socket.sendto(msg.dgram, (self.host, self.port))
            
        except ImportError:
            # Fallback: simple message format if python-osc not available
            message = f"{address} {' '.join(str(a) for a in args)}".encode("utf-8")
            self._socket.sendto(message, (self.host, self.port))
        except Exception as e:
            logger.error(f"UDP send error: {e}")


class AbletonConnection:
    """Hybrid TCP/UDP connection manager for Ableton Live.

    Provides unified interface for both reliable and low-latency
    communication with the Ableton Remote Script.

    Enterprise Features:
        - Automatic reconnection with exponential backoff
        - Connection health monitoring
        - Graceful degradation to offline mode
    """

    def __init__(self, retry_config: RetryConfig | None = None):
        """Initialize connection manager.

        Args:
            retry_config: Optional retry configuration. Uses defaults if not provided.
        """
        self.tcp = TCPConnection()
        self.udp = UDPConnection()
        self._connected = False
        self._retry_config = retry_config or RetryConfig()
        self._health = ConnectionHealth()

    async def connect(self) -> bool:
        """Connect to Ableton via both TCP and UDP.

        Returns:
            True if TCP connection succeeded (UDP is optional)
        """
        tcp_ok = await self.tcp.connect()
        udp_ok = await self.udp.connect()

        self._connected = tcp_ok

        if tcp_ok:
            logger.info("Ableton connection established")
            self._health.record_success()
        else:
            logger.warning("Running in offline mode (no Ableton connection)")
            self._health.record_failure()

        return tcp_ok

    async def disconnect(self):
        """Disconnect from Ableton."""
        await self.tcp.disconnect()
        await self.udp.disconnect()
        self._connected = False
        self._health.connected = False

    @property
    def is_connected(self) -> bool:
        """Check if connected to Ableton."""
        return self._connected

    @property
    def health(self) -> ConnectionHealth:
        """Get connection health metrics."""
        return self._health

    async def reconnect(self) -> bool:
        """Attempt to reconnect to Ableton.

        Returns:
            True if reconnection succeeded.
        """
        logger.info("Attempting to reconnect to Ableton...")
        self._health.record_reconnect()

        await self.disconnect()
        return await self.connect()

    async def send_command(self, command: str, params: dict | None = None) -> dict:
        """Send reliable command via TCP with automatic retry.

        Args:
            command: Command type name
            params: Optional parameters

        Returns:
            Response dictionary
        """
        if not self._connected:
            # Return mock data for offline mode
            return self._get_mock_response(command, params)

        async def _send():
            return await self.tcp.send(command, params)

        try:
            result = await retry_with_backoff(
                _send,
                self._retry_config,
                f"Command '{command}'",
            )
            self._health.record_success()
            return result
        except (ConnectionError, TimeoutError) as e:
            self._health.record_failure()

            # After max retries, attempt full reconnection
            if self._health.consecutive_failures >= self._retry_config.max_retries:
                logger.warning("Max failures reached, attempting full reconnection...")
                if await self.reconnect():
                    # Try one more time after reconnection
                    try:
                        return await self.tcp.send(command, params)
                    except Exception:
                        pass

            # Fall back to offline mode
            logger.warning(f"Falling back to offline mode for command: {command}")
            self._connected = False
            return self._get_mock_response(command, params)
    
    def send_realtime(self, address: str, *args: Any):
        """Send low-latency message via UDP/OSC.
        
        Args:
            address: OSC address
            *args: OSC arguments
        """
        self.udp.send_osc(address, *args)
    
    def _get_mock_response(self, command: str, params: dict | None) -> dict:
        """Generate mock response for offline mode.
        
        Used for development and testing without Ableton running.
        """
        mock_responses = {
            "get_session_info": {
                "tempo": 120.0,
                "time_signature": {"numerator": 4, "denominator": 4},
                "track_count": {"midi": 0, "audio": 0, "return": 2, "master": 1},
                "is_playing": False,
                "offline_mode": True,
            },
            "create_midi_track": {
                "index": params.get("index", 0) if params else 0,
                "name": params.get("name", "MIDI Track") if params else "MIDI Track",
                "type": "midi",
            },
            "create_audio_track": {
                "index": params.get("index", 0) if params else 0,
                "name": params.get("name", "Audio Track") if params else "Audio Track",
                "type": "audio",
            },
        }
        
        return mock_responses.get(command, {"status": "ok", "offline_mode": True})
