"""Opt-in verified release through actual Docker stdio and ordinary networking."""

from __future__ import annotations

import importlib.util
import json
import os
import re
import subprocess
import sys
import uuid
from pathlib import Path

import pytest


def test_verified_container_and_exported_bridge_use_production_json(tmp_path):
    """The external boundary alone is modelled; released transport bytes are executed."""
    if os.environ.get("SUNNY_RELEASE_PROTOCOL_TESTS") != "1":
        pytest.skip("Enable the local released protocol witness explicitly")
    release = Path(os.environ["SUNNY_RELEASE_PROTOCOL_DIRECTORY"])
    trusted = os.environ["SUNNY_RELEASE_PROTOCOL_MANIFEST_SHA256"]
    image = os.environ["SUNNY_RELEASE_PROTOCOL_IMAGE"]
    assert re.fullmatch(r"sha256:[0-9a-f]{64}", image)
    project = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location(
        "released_protocol_verifier", project / "tools/release.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    manifest = module.verify_release(release, trusted)
    assert image == manifest["image"]["local_immutable_id"], (
        "Use the verified producing-store image"
    )
    owner = uuid.uuid4().hex
    container = "sunny-released-protocol-fixture-" + owner

    def docker(*arguments):
        return subprocess.run(["docker", *arguments], capture_output=True, text=True, timeout=15)

    assert not os.environ.get("DOCKER_HOST") or os.environ["DOCKER_HOST"].startswith(
        ("unix://", "npipe://")
    ), "Local Docker engine only"
    context = docker("context", "inspect")
    assert context.returncode == 0
    assert json.loads(context.stdout)[0]["Endpoints"]["docker"]["Host"].startswith(
        ("unix://", "npipe://")
    )
    daemon = docker("info", "--format", "{{.ID}}")
    assert daemon.returncode == 0 and daemon.stdout.strip()
    inspection = docker("image", "inspect", image)
    assert inspection.returncode == 0, inspection.stderr
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    try:
        observed = subprocess.run(
            [
                sys.executable,
                str(project / "tests/python/released_protocol_driver.py"),
                "--release",
                str(release),
                "--image",
                image,
                "--directory",
                str(tmp_path),
                "--container",
                container,
                "--owner",
                owner,
            ],
            capture_output=True,
            text=True,
            timeout=90,
            env=env,
        )
        assert observed.returncode == 0, (observed.stdout, observed.stderr)
        result = json.loads(observed.stdout)
        assert result["exported_bridge_source_sha256"] == manifest["bridge"]["source_sha256"]
        # One four-beat chord uses the documented 90% gate: 4 * .9 = 3.6.
        assert result["literal_notes"] == [[60, 0.0, 3.6], [64, 0.0, 3.6], [67, 0.0, 3.6]]
        assert result["native_host_executed"] is False
        assert (
            result["read_only_doctor"]
            and result["occupied_slot_preserved"]
            and result["incremental_log"]
        )
        assert result["configuration_schema_version"] == 1
        assert result["lost_reply_recovered_by_original_token"] is True
        assert result["musical_setter_entries"] == 1
        assert result["durable_fence_before_native_dispatch"] is True
        assert docker("inspect", container).returncode != 0, (
            "Owned container survived input closure"
        )
        assert docker("inspect", container + "-restart").returncode != 0
        # Loading the export must not add bytecode or modify any paired file.
        assert module.verify_release(release, trusted) == manifest
    finally:
        assert docker("info", "--format", "{{.ID}}").stdout == daemon.stdout, (
            "Docker daemon changed"
        )
        for name in (
            container,
            container + "-restart",
            container + "-observe-1",
            container + "-observe-2",
        ):
            remaining = docker("inspect", name)
            if remaining.returncode == 0:
                item = json.loads(remaining.stdout)[0]
                assert item["Config"]["Labels"].get("com.sunny.fixture.owner") == owner
                removed = docker("rm", "-f", name)
                assert removed.returncode == 0, removed.stderr
