"""Finite developer-kit I/O and owned-process lifetime, without native Live or SSH."""

from __future__ import annotations

import json
import os
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path
from types import ModuleType

import pytest
from test_live_end_to_end import _sunny_mcp_binary


@pytest.fixture
def kit(monkeypatch):
    """Read the maintained helper and keep test environment selectors explicit."""
    path = Path(__file__).resolve().parents[2] / "tools/live_qualification/common.py"
    module = ModuleType("qualification_io_test")
    module.__file__ = str(path)
    exec(compile(path.read_bytes(), str(path), "exec"), module.__dict__)
    for key in (
        "SUNNY_ABLETON_HOST",
        "SUNNY_TCP_PORT",
        "SUNNY_BIND_HOST",
        "SUNNY_WORKSPACE_PATH",
        "SUNNY_WORKSPACE_RECOVERY",
        "SUNNY_CONFIG_PATH",
    ):
        monkeypatch.delenv(key, raising=False)
    # Release authentication and launch grammar have their own literal tests.
    # This fixture isolates real owned child/pipe I/O behind those two validators.
    monkeypatch.setattr(module, "verify_selected_release", lambda configuration: None)
    monkeypatch.setattr(module, "qualification_command", lambda configuration, image: None)
    return module


def test_frame_header_and_body_share_one_absolute_budget(kit):
    """A peer cannot extend a response indefinitely by sending each byte before an idle timeout."""
    left, right = socket.socketpair()
    done = threading.Event()

    def trickle():
        try:
            for value in struct.pack(">I", 6) + b"123456":
                if done.wait(0.06):
                    return
                right.sendall(bytes([value]))
        except OSError:
            return

    thread = threading.Thread(target=trickle)
    thread.start()
    started = time.monotonic()
    try:
        with pytest.raises(TimeoutError):
            kit.recv_frame(left, started + 0.3)
        assert time.monotonic() - started < 0.7
    finally:
        done.set()
        left.close()
        right.close()
        thread.join(timeout=1)
        assert not thread.is_alive()


def test_expired_dns_budget_kills_and_reaps_only_its_resolver(kit, monkeypatch):
    """A real deliberately hung resolver process is dead when the caller receives its failure."""
    actual = subprocess.Popen
    children = []

    def hung(arguments, **kwargs):
        changed = list(arguments)
        changed[3] = "import time; time.sleep(60)"
        child = actual(changed, **kwargs)
        children.append(child)
        return child

    monkeypatch.setattr(kit.subprocess, "Popen", hung)
    started = time.monotonic()
    with pytest.raises(subprocess.TimeoutExpired):
        kit.addresses("localhost", 9001, started + 0.15)
    assert time.monotonic() - started < 1.5
    assert len(children) == 1 and children[0].poll() is not None


def test_numeric_address_needs_no_dns_and_typed_endpoint_precedes_effects(kit, monkeypatch):
    """Direct IPv4 routing stays local and boolean ports cannot launch resolver work."""
    monkeypatch.setattr(
        kit.subprocess, "Popen", lambda *a, **k: pytest.fail("Unexpected DNS child")
    )
    assert kit.addresses("127.0.0.1", 9001, time.monotonic() + 1) == [("127.0.0.1", 9001)]
    with pytest.raises(ValueError):
        kit.addresses("localhost", True, time.monotonic() + 1)


def test_actual_mcp_uses_json_and_owns_its_process_group(kit, tmp_path, monkeypatch):
    """The actual server negotiates MCP and returns the literal authored value before clean EOF."""
    if os.name != "posix":
        pytest.skip("Developer qualification uses POSIX Python")
    configuration = tmp_path / "offline configuration.json"
    configuration.write_text(
        json.dumps(
            {
                "configuration_schema_version": 1,
                "client": {"transport": {"mode": "offline"}, "workspace": None},
            }
        )
    )
    monkeypatch.setenv("SUNNY_CONFIG_PATH", str(configuration))
    log = tmp_path / "qualification.jsonl"
    with kit.Mcp(
        {"mcp_command": [str(_sunny_mcp_binary())], "image_local_immutable_id": "io-fixture"}, log
    ) as client:
        score = client.call(
            "score_create",
            title="Literal kit phrase",
            total_bars=1,
            parts=[{"name": "Piano", "instrument_type": 47}],
        )["score_id"]
        assert (
            client.call("score_get_json", score_id=score)["metadata"]["title"]
            == "Literal kit phrase"
        )
    rows = [json.loads(line) for line in log.read_text().splitlines()]
    cleanup = rows[-1]["cleanup"]
    assert cleanup["success"] is True and cleanup["process_group_empty"] is True
    assert client.process.poll() == 0
    assert rows[0]["request"]["params"]["protocolVersion"] == "2025-11-25"


def test_unread_child_stdin_cannot_block_a_musical_request(kit, tmp_path):
    """An actual backpressured pipe expires and the owned child is reaped without retry."""
    if os.name != "posix":
        pytest.skip("Developer qualification uses POSIX Python")
    script = tmp_path / "unread-input.py"
    script.write_text(
        "import json,sys,time\n"
        "request=json.loads(sys.stdin.readline())\n"
        "print(json.dumps({'jsonrpc':'2.0','id':request['id'],"
        "'result':{'protocolVersion':'2025-11-25'}}),flush=True)\n"
        "time.sleep(60)\n"
    )
    log = tmp_path / "uncertain.jsonl"
    client = kit.Mcp(
        {
            "mcp_command": [sys.executable, str(script)],
            "mcp_timeout": 2,
            "image_local_immutable_id": "io-fixture",
        },
        log,
    )
    client.timeout = 0.15
    started = time.monotonic()
    try:
        with pytest.raises(TimeoutError):
            client.call("literal_unexecuted_fixture", value="x" * (256 * 1024))
        assert time.monotonic() - started < 0.8
    finally:
        client.close()
    rows = [json.loads(line) for line in log.read_text().splitlines()]
    failed = next(row for row in rows if row["kind"] == "mcp_response_unavailable")
    assert 0 < failed["bytes_written_to_owned_pipe"] < 256 * 1024
    assert "not native effect evidence" in failed["action"]
    assert rows[-1]["cleanup"]["success"] is True and client.process.poll() is not None
