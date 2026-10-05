"""Opt-in real Desktop networking witness; no SSH, native Live or security changes."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
import time
import uuid
from pathlib import Path

import pytest


def test_desktop_container_reaches_windows_loopback():
    """Prove the boundary used by the selected external Windows SSH forward."""
    if os.environ.get("SUNNY_DOCKER_NETWORK_TESTS") != "1":
        pytest.skip("Enable the authorized local Desktop network witness explicitly")
    image = os.environ.get("SUNNY_DOCKER_NETWORK_IMAGE", "")
    assert re.fullmatch(r"sha256:[0-9a-f]{64}", image), "Supply an immutable local image ID"
    powershell, docker = shutil.which("powershell.exe"), shutil.which("docker")
    if powershell is None or docker is None:
        pytest.skip("Windows PowerShell and Docker Desktop are required")
    endpoint = os.environ.get("DOCKER_HOST", "")
    assert not endpoint or endpoint.startswith(("unix://", "npipe://")), "Local daemon only"
    context = json.loads(subprocess.check_output([docker, "context", "inspect"], text=True))[0]
    assert context["Endpoints"]["docker"]["Host"].startswith(("unix://", "npipe://"))
    information = json.loads(
        subprocess.check_output([docker, "info", "--format", "{{json .}}"], text=True, timeout=10)
    )
    assert "Docker Desktop" in information["OperatingSystem"]
    daemon = information["ID"]
    convert = lambda path: (  # noqa: E731
        str(path)
        if os.name == "nt"
        else subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()
    )
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
    owner = uuid.uuid4().hex
    container = "sunny-loopback-fixture-" + owner
    with tempfile.TemporaryDirectory(prefix="Sunny-loopback-fixture-", dir=local_temp) as directory:
        root = Path(directory)
        script, ready = root / "listen.ps1", root / "port.txt"
        script.write_text(
            """param([string]$ReadyPath)
$ErrorActionPreference = 'Stop'
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
$listener.Start()
try {
    [IO.File]::WriteAllText($ReadyPath, [string]$listener.LocalEndpoint.Port)
    $deadline = [DateTime]::UtcNow.AddSeconds(25)
    while (-not $listener.Pending()) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Fixture connection timed out' }
        Start-Sleep -Milliseconds 50
    }
    $client = $listener.AcceptTcpClient()
    try {
        $bytes = [Text.Encoding]::ASCII.GetBytes("sunny-loopback-fixture`n")
        $stream = $client.GetStream()
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $client.Dispose() }
} finally { $listener.Stop() }
""",
            encoding="utf-8-sig",
        )
        process = subprocess.Popen(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                convert(script),
                "-ReadyPath",
                convert(ready),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            end = time.monotonic() + 10
            while not ready.exists():
                assert process.poll() is None, process.communicate()
                assert time.monotonic() < end, "Windows fixture startup deadline"
                time.sleep(0.05)
            port = int(ready.read_text())
            assert 1 <= port <= 65535
            command = (
                f"exec 3<>/dev/tcp/host.docker.internal/{port}; "
                'read -r observed <&3; test "$observed" = sunny-loopback-fixture'
            )
            result = subprocess.run(
                [
                    docker,
                    "run",
                    "--pull=never",
                    "--rm",
                    "--name",
                    container,
                    "--label",
                    "com.sunny.fixture.owner=" + owner,
                    "--entrypoint",
                    "bash",
                    image,
                    "-c",
                    command,
                ],
                capture_output=True,
                text=True,
                timeout=15,
            )
            assert result.returncode == 0, result.stderr
            _, error = process.communicate(timeout=5)
            assert process.returncode == 0, error
        finally:
            if process.poll() is None:
                process.terminate()
                process.communicate(timeout=5)
            observed_daemon = subprocess.check_output(
                [docker, "info", "--format", "{{.ID}}"], text=True, timeout=10
            ).strip()
            assert observed_daemon == daemon, "Daemon changed; do not select cleanup elsewhere"
            identifiers = subprocess.check_output(
                [
                    docker,
                    "ps",
                    "-aq",
                    "--no-trunc",
                    "--filter",
                    "label=com.sunny.fixture.owner=" + owner,
                ],
                text=True,
                timeout=10,
            ).splitlines()
            assert len(identifiers) <= 1
            for identifier in identifiers:
                assert re.fullmatch(r"[0-9a-f]{64}", identifier)
                subprocess.run([docker, "rm", "--force", identifier], check=True, timeout=10)
            remaining = subprocess.check_output(
                [docker, "ps", "-aq", "--filter", "label=com.sunny.fixture.owner=" + owner],
                text=True,
                timeout=10,
            )
            assert remaining.strip() == "", "Fixture container cleanup failed"
