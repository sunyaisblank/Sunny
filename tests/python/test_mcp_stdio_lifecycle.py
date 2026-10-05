"""Exercise bounded production stdio with real pipes and an unread or closed client."""

from __future__ import annotations

import json
import os
import selectors
import subprocess
import time
from contextlib import contextmanager

from test_live_end_to_end import _sunny_mcp_binary


def send(process: subprocess.Popen, request: dict) -> None:
    """Keep client input open while independently controlling output ownership."""
    process.stdin.write(json.dumps(request).encode() + b"\n")
    process.stdin.flush()


@contextmanager
def running():
    """Launch one actual offline executable and always reap its exact child."""
    environment = {key: value for key, value in os.environ.items() if not key.startswith("SUNNY_")}
    process = subprocess.Popen(
        [str(_sunny_mcp_binary())],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
    )
    try:
        send(
            process,
            {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {"protocolVersion": "2025-11-25"},
            },
        )
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            payload = b""
            deadline = time.monotonic() + 3
            while b"\n" not in payload:
                assert selector.select(max(0, deadline - time.monotonic())), "No initialize reply"
                part = os.read(process.stdout.fileno(), 4096)
                assert part, "Sunny exited before initialize"
                payload += part
        assert json.loads(payload)["id"] == 1
        yield process
    finally:
        if process.poll() is None:
            process.kill()
        process.wait(timeout=3)
        for pipe in (process.stdin, process.stdout, process.stderr):
            if not pipe.closed:
                pipe.close()


def test_unread_stdout_expires_and_reaps_while_stdin_remains_open():
    """An inventory larger than the actual OS pipe must trigger the finite output deadline."""
    with running() as process:
        send(process, {"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        started = time.monotonic()
        assert process.wait(timeout=8) == 1
        assert time.monotonic() - started < 7
        assert not process.stdin.closed
        assert b"bounded I/O" in process.stderr.read()


def test_closed_stdout_is_contained_and_revokes_the_open_input_session():
    """EPIPE must become a normal diagnosed failure, with no signal-killed process."""
    with running() as process:
        process.stdout.close()
        send(process, {"jsonrpc": "2.0", "id": 2, "method": "ping"})
        assert process.wait(timeout=3) == 1
        assert not process.stdin.closed
        assert b"bounded I/O" in process.stderr.read()


def test_idle_input_eof_releases_the_owned_session_promptly():
    """The normal client disconnect keeps successful initialization and exits cleanly."""
    with running() as process:
        process.stdin.close()
        assert process.wait(timeout=3) == 0
        assert process.stdout.read() == b""
        diagnostics = process.stderr.read()
        assert b"running offline" in diagnostics
        assert b"fatal" not in diagnostics
