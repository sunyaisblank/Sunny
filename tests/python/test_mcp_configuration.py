"""Configuration errors must refuse before MCP startup, in both peers."""

from __future__ import annotations

import os
import subprocess

import pytest
from test_live_end_to_end import _sunny_mcp_binary


@pytest.mark.parametrize(
    "value",
    ["", "0", "65536", "+9001", "-1", "09001", " 9001", "9001 ", "９００１", "1.0", "0x2329"],
)
def test_invalid_native_port_has_no_fallback(value):
    """Malformed endpoints must not silently connect through the default port."""
    environment = {key: value for key, value in os.environ.items() if not key.startswith("SUNNY_")}
    environment.update(SUNNY_ABLETON_HOST="127.0.0.1", SUNNY_TCP_PORT=value)
    result = subprocess.run(
        [str(_sunny_mcp_binary())],
        env=environment,
        input='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}\n',
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert result.returncode == 1
    assert result.stdout == ""
    assert "SUNNY_TCP_PORT must be an ASCII decimal port from 1 to 65535" in result.stderr
    assert "could not connect" not in result.stderr


def test_empty_native_host_refuses():
    """An explicitly empty host must not become an unintended resolver query."""
    environment = {key: value for key, value in os.environ.items() if not key.startswith("SUNNY_")}
    environment["SUNNY_ABLETON_HOST"] = ""
    result = subprocess.run(
        [str(_sunny_mcp_binary())],
        env=environment,
        input="",
        capture_output=True,
        text=True,
        timeout=5,
    )
    assert result.returncode == 1
    assert result.stdout == ""
    assert "SUNNY_ABLETON_HOST must not be empty" in result.stderr
