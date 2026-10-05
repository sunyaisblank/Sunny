"""Opt-in real Live smoke check from one verified production release.

Set SUNNY_LIVE_QUALIFICATION_CONFIG to an external copy of the release's
qualification configuration and SUNNY_LIVE_SCRATCH_APPROVED=1 only after
approving its saved disposable Set and original Bridge/Document tokens.
The exact exported POSIX developer kit supplies the production Docker client.
This two-Part check leaves its authored material for independent observation;
it does not satisfy the remaining 23 Live/UI/audio validation groups.
"""

from __future__ import annotations

import importlib.util
import json
import os
import sys
from pathlib import Path
from types import ModuleType

import pytest
from live_scenario import run_live_smoke

CONFIGURATION = os.environ.get("SUNNY_LIVE_QUALIFICATION_CONFIG")


@pytest.mark.skipif(not CONFIGURATION, reason="select an external real Live qualification JSON")
def test_project_deploys_to_a_real_live_set(monkeypatch) -> None:
    """Verify release, original approval and scratch preconditions before any authoring."""
    assert os.environ.get("SUNNY_LIVE_SCRATCH_APPROVED") == "1", (
        "Explicitly approve the configured disposable scratch Set before this mutating check"
    )
    config = json.loads(Path(CONFIGURATION).read_text(encoding="utf-8"))
    release = Path(config["release_directory"])
    project = Path(__file__).resolve().parents[2]
    specification = importlib.util.spec_from_file_location(
        "real_live_release_verifier", project / "tools/release.py"
    )
    verifier = importlib.util.module_from_spec(specification)
    exec(
        compile((project / "tools/release.py").read_bytes(), str(specification.origin), "exec"),
        verifier.__dict__,
    )
    verifier.verify_release(release, config["expected_release_manifest_sha256"])
    # Authenticate all operator bytes before executing the image-owned kit.
    monkeypatch.setattr(sys, "dont_write_bytecode", True)
    kit = release / "operator/live_qualification"
    common = ModuleType("common")
    common.__file__ = str(kit / "common.py")
    exec(compile((kit / "common.py").read_bytes(), common.__file__, "exec"), common.__dict__)
    monkeypatch.setitem(sys.modules, "common", common)
    runner = ModuleType("real_live_host_runner")
    runner.__file__ = str(kit / "host_runner.py")
    exec(compile((kit / "host_runner.py").read_bytes(), runner.__file__, "exec"), runner.__dict__)
    common.verify_selected_release(config)
    output = Path(config["evidence_directory"])
    output.mkdir(parents=True, exist_ok=True)
    log = output / "real-live-smoke.jsonl"
    assert not Path(config["host_workspace"]).exists(), (
        "Select a new disposable workspace; preserve every earlier run and its native history"
    )
    runner.scratch(config, log, True)
    with common.Mcp(config, log) as client:
        common.prime_native_session(client, config)
        saved = client.call("workspace_save", path=config["server_workspace"])
        assert (
            saved.get("durability_confirmed") is True
            and saved.get("native_history_available") is True
        ), saved
        observed = run_live_smoke(client)
        saved = client.call("workspace_save", path=config["server_workspace"])
        assert saved.get("durability_confirmed") is True, saved
    common.record(log, "real_live_smoke_observation", observation=observed)
    verifier.verify_release(release, config["expected_release_manifest_sha256"])
    print(json.dumps(observed, indent=2, default=str))
