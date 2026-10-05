"""Check Windows native handle inheritance and exact local Docker resource ownership.

This exercises launcher primitives against an existing immutable local image.
It does not establish SSH trust, a native Live endpoint, or the final paired release.
"""

from __future__ import annotations

import base64
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]


def test_actual_windows_client_owned_docker_and_protocol_handles():
    """Leave only literal MCP output while cleanup preserves an unrelated container."""
    if os.environ.get("SUNNY_DOCKER_CLIENT_TESTS") != "1":
        pytest.skip("explicit opt-in is required for scoped local Docker fixtures")
    image = os.environ.get("SUNNY_DOCKER_CLIENT_IMAGE", "")
    assert re.fullmatch(r"sha256:[0-9a-f]{64}", image)
    powershell = shutil.which("powershell.exe")
    if powershell is None:
        pytest.skip("Windows PowerShell and Docker Desktop are required")
    driver = (ROOT / "tests/windows/client_test.ps1").read_text()
    for marker, name in (
        ("__INSTALLER__", "Sunny.ps1"),
        ("__REMOTE__", "SunnyRemote.ps1"),
        ("__CLIENT__", "SunnyClient.ps1"),
    ):
        driver = driver.replace(
            marker, base64.b64encode((ROOT / "tools/windows" / name).read_bytes()).decode()
        )
    driver = driver.replace("__IMAGE__", image)
    windows_temp = subprocess.check_output(
        [powershell, "-NoProfile", "-NonInteractive", "-Command", "[IO.Path]::GetTempPath()"],
        text=True,
        timeout=10,
    ).strip()
    local_temp = (
        windows_temp
        if os.name == "nt"
        else subprocess.check_output(["wslpath", "-u", windows_temp], text=True).strip()
    )
    with tempfile.TemporaryDirectory(prefix="Sunny-client-driver-", dir=local_temp) as directory:
        script = Path(directory) / "test.ps1"
        script.write_text(driver, encoding="utf-8-sig")
        selected = (
            str(script)
            if os.name == "nt"
            else subprocess.check_output(["wslpath", "-w", str(script)], text=True).strip()
        )
        literal = (
            '{"jsonrpc":"2.0","id":901,"method":"initialize",'
            '"params":{"protocolVersion":"2025-11-25","capabilities":{},'
            '"clientInfo":{"name":"windows-handles-fixture","version":"1"}}}\n'
        )
        result = subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-File", selected],
            input=literal,
            capture_output=True,
            text=True,
            timeout=90,
        )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "SUNNY_CLIENT_FIXTURE_PASS" in result.stderr
    reply = json.loads(result.stdout)
    assert reply["jsonrpc"] == "2.0" and reply["id"] == 901
    assert reply["result"]["protocolVersion"] == "2025-11-25"
    assert reply["result"]["serverInfo"]["name"] == "sunny-mcp"
