"""Minimal OSC 1.0 Codec.

Component: HSOSC001A
Domain: HS (Host) | Category: OSC (Open Sound Control)

Allocation-free OSC encoder/decoder for Python. Implements the
subset of OSC 1.0 needed for AbletonOSC-compatible communication.

Invariants:
- All strings 4-byte aligned with null padding
- Big-endian encoding for int32 and float32
- Encode-decode round-trip preserves all values exactly
"""

from __future__ import annotations

import struct
from typing import Any


# =============================================================================
# OSC Encoding
# =============================================================================


def _pad_to_4(data: bytes) -> bytes:
    """Pad data to 4-byte boundary with null bytes."""
    remainder = len(data) % 4
    if remainder == 0:
        return data
    return data + b"\x00" * (4 - remainder)


def _encode_string(s: str) -> bytes:
    """Encode OSC string: null-terminated, 4-byte padded."""
    raw = s.encode("ascii") + b"\x00"
    return _pad_to_4(raw)


def encode_message(address: str, args: list[Any] | None = None) -> bytes:
    """Encode an OSC message.

    Args:
        address: OSC address pattern (e.g., "/live/song/set/tempo").
        args: List of arguments. Supported types:
            int → 'i' (int32)
            float → 'f' (float32)
            str → 's' (string)
            bytes → 'b' (blob)

    Returns:
        Encoded OSC message bytes.

    Invariants:
        - Address must start with '/'
        - All components 4-byte aligned
    """
    if not address.startswith("/"):
        raise ValueError(f"OSC address must start with '/': {address}")

    parts = [_encode_string(address)]

    if args is None:
        args = []

    # Build type tag string
    type_chars = [","]
    arg_data = []

    for arg in args:
        if isinstance(arg, int):
            type_chars.append("i")
            arg_data.append(struct.pack(">i", arg))
        elif isinstance(arg, float):
            type_chars.append("f")
            arg_data.append(struct.pack(">f", arg))
        elif isinstance(arg, str):
            type_chars.append("s")
            arg_data.append(_encode_string(arg))
        elif isinstance(arg, (bytes, bytearray)):
            type_chars.append("b")
            blob = bytes(arg)
            arg_data.append(struct.pack(">i", len(blob)) + _pad_to_4(blob))
        else:
            raise TypeError(f"Unsupported OSC argument type: {type(arg)}")

    parts.append(_encode_string("".join(type_chars)))
    parts.extend(arg_data)

    return b"".join(parts)


# =============================================================================
# OSC Decoding
# =============================================================================


class OscMessage:
    """Decoded OSC message."""

    __slots__ = ("address", "args")

    def __init__(self, address: str, args: list[Any]) -> None:
        self.address = address
        self.args = args

    def __repr__(self) -> str:
        return f"OscMessage({self.address!r}, {self.args!r})"


def _read_string(data: bytes, offset: int) -> tuple[str, int]:
    """Read null-terminated, 4-byte padded string."""
    end = data.index(b"\x00", offset)
    s = data[offset:end].decode("ascii")
    # Advance past null terminator and padding
    padded_end = end + 1
    remainder = padded_end % 4
    if remainder != 0:
        padded_end += 4 - remainder
    return s, padded_end


def decode_message(data: bytes) -> OscMessage:
    """Decode an OSC message.

    Args:
        data: Raw OSC message bytes.

    Returns:
        Decoded OscMessage with address and arguments.

    Raises:
        ValueError: If the message is malformed.
    """
    if len(data) < 4:
        raise ValueError("OSC message too short")

    # Read address
    address, offset = _read_string(data, 0)
    if not address.startswith("/"):
        raise ValueError(f"Invalid OSC address: {address}")

    # Read type tag string
    if offset >= len(data) or data[offset:offset + 1] != b",":
        # No type tag — message with no arguments
        return OscMessage(address, [])

    type_tag, offset = _read_string(data, offset)
    tags = type_tag[1:]  # Strip leading comma

    # Parse arguments
    args: list[Any] = []
    for tag in tags:
        if tag == "i":
            value = struct.unpack_from(">i", data, offset)[0]
            offset += 4
            args.append(value)
        elif tag == "f":
            value = struct.unpack_from(">f", data, offset)[0]
            offset += 4
            args.append(value)
        elif tag == "s":
            value, offset = _read_string(data, offset)
            args.append(value)
        elif tag == "b":
            blob_size = struct.unpack_from(">i", data, offset)[0]
            offset += 4
            blob = data[offset:offset + blob_size]
            padded_size = blob_size
            remainder = padded_size % 4
            if remainder != 0:
                padded_size += 4 - remainder
            offset += padded_size
            args.append(blob)
        else:
            raise ValueError(f"Unsupported OSC type tag: {tag}")

    return OscMessage(address, args)


__all__ = [
    "encode_message",
    "decode_message",
    "OscMessage",
]
