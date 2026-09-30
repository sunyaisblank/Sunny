"""Tests for evidence-empty Max scaffolding and the named-Live MCP driver."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import zipfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
VALIDATION = REPOSITORY / "max-package" / "misc" / "validation"
PREPARE = VALIDATION / "prepare-max-host-run.py"
LIVE_RUNNER = VALIDATION / "run-named-live-validation.py"


def _package_archive(root: Path, platform: str, architecture: str) -> tuple[Path, Path]:
    package = root / "Sunny"
    validation = package / "misc" / "validation"
    validation.mkdir(parents=True)
    (validation / "export-max-test-sqlite.py").write_text("# extractor\n", encoding="utf-8")
    (validation / "max-test-harness.json").write_text("{}\n", encoding="utf-8")
    for name in ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"):
        binary = (
            package / "externals" / f"{name}.mxe64"
            if platform == "windows"
            else package / "externals" / f"{name}.mxo" / "Contents" / "MacOS" / name
        )
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(f"binary:{name}\n".encode())

    archive = root / f"Sunny-0.4.0-{platform}-{architecture}.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                zipped.write(path, f"Sunny/{path.relative_to(package).as_posix()}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(f"{archive}.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    return package, archive


def _prepare_command(
    package: Path, archive: Path, output: Path, cell: str, *, live: bool = False
) -> list[str]:
    command = [
        sys.executable,
        str(PREPARE),
        "--cell",
        cell,
        "--archive",
        str(archive),
        "--package-root",
        str(package),
        "--output-root",
        str(output),
        "--source-revision",
        "1234567890abcdef1234567890abcdef12345678",
        "--observed-at-utc",
        "2026-09-01T04:00:00Z",
        "--max-version",
        "9.0.5",
        "--audio-driver",
        "Named Driver",
    ]
    if live:
        command.extend(["--live-version", "12.3.5", "--max-for-live-version", "9.0.5"])
    return command


def test_prepare_max_host_run_creates_only_empty_standalone_facts(tmp_path: Path) -> None:
    """A standalone scaffold contains provenance and applicability, never host success."""
    package, archive = _package_archive(tmp_path / "input", "windows", "x86_64")
    output = tmp_path / "release"
    completed = subprocess.run(
        _prepare_command(package, archive, output, "windows-x86_64-standalone-max"),
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 0, completed.stderr
    cell = output / "cells" / "windows-x86_64-standalone-max"
    observation = json.loads((cell / "observation.json").read_text(encoding="utf-8"))
    assert observation["environment"]["host_kind"] == "standalone_max"
    assert observation["environment"]["live_version"] is None
    assert all(not artifact["discovered"] for artifact in observation["artifacts"])
    outcomes = {check["check"]: check["outcome"] for check in observation["checks"]}
    assert outcomes["live_transport_discontinuities"] == "not_applicable"
    assert outcomes["standalone_transport_identity"] == "not_run"
    assert set(outcomes.values()) == {"not_run", "not_applicable"}
    assert all(check["evidence_relative_path"] is None for check in observation["checks"])

    inputs = json.loads((cell / "operator-inputs.json").read_text(encoding="utf-8"))
    assert inputs["max_test_applicable"] is True
    assert inputs["commands_after_host_exit"]["extract_max_test"][-1] == ("<positive-test-id>")
    assert inputs["commands_after_host_exit"]["index_host_run"][1].endswith("index-max-host-run.py")
    assert inputs["commands_after_host_exit"]["apply_host_run"][1] == "apply-host-run"
    assert inputs["commands_after_host_exit"]["verify_host_run"][1] == "verify-host-run"
    assert inputs["commands_after_host_exit"]["host_run_result_output"].endswith(
        "evidence/host-run-result.json"
    )
    assert not (output / "records" / "windows-x86_64-standalone-max.json").exists()

    repeated = subprocess.run(
        _prepare_command(package, archive, output, "windows-x86_64-standalone-max"),
        check=False,
        capture_output=True,
        text=True,
    )
    assert repeated.returncode == 2
    assert "refusing to overwrite" in repeated.stderr


def test_prepare_max_host_run_closes_m4l_applicability(tmp_path: Path) -> None:
    """An M4L scaffold flips only the two complementary transport obligations."""
    package, archive = _package_archive(tmp_path / "input", "macos", "arm64")
    output = tmp_path / "release"
    completed = subprocess.run(
        _prepare_command(package, archive, output, "macos-arm64-max-for-live", live=True),
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 0, completed.stderr
    cell = output / "cells" / "macos-arm64-max-for-live"
    observation = json.loads((cell / "observation.json").read_text(encoding="utf-8"))
    outcomes = {check["check"]: check["outcome"] for check in observation["checks"]}
    assert observation["environment"]["live_version"] == "12.3.5"
    assert outcomes["live_transport_discontinuities"] == "not_run"
    assert outcomes["standalone_transport_identity"] == "not_applicable"
    inputs = json.loads((cell / "operator-inputs.json").read_text(encoding="utf-8"))
    assert inputs["max_test_applicable"] is False
    assert inputs["m4l_device_source"]["device_kind"] == "max_audio_effect"
    assert inputs["m4l_device_source"]["prepare"][1].endswith("prepare-m4l-device-source.py")
    assert inputs["m4l_device_source"]["authority"].endswith("save and freeze the retained .amxd.")
    assert inputs["commands_after_host_exit"]["apply_max_test"] is None
    assert inputs["commands_after_host_exit"]["export_m4l_assertions"][1].endswith(
        "export-m4l-assertions.py"
    )
    assert inputs["commands_after_host_exit"]["apply_host_run"][1] == "apply-host-run"
    assert (
        cell / "evidence/live-transport-discontinuities/max-test-harness.json"
    ).read_bytes() == (VALIDATION / "max-test-harness.json").read_bytes()
    scenario_template = json.loads(
        (cell / "scenario-results.template.json").read_text(encoding="utf-8")
    )
    assert scenario_template["assertion_run_id"] == "<replace-with-exact-marked-run-id>"
    assert [scenario["name"] for scenario in scenario_template["scenarios"]] == [
        "stop_start",
        "seek_forward_and_backward",
        "arrangement_loop",
        "tempo_change",
        "preview_enter_exit",
        "scheduler_settings",
    ]
    assert all(not scenario["completed"] for scenario in scenario_template["scenarios"])


def test_prepare_max_host_run_rejects_digest_drift(tmp_path: Path) -> None:
    """Archive-sidecar disagreement fails before an output workspace exists."""
    package, archive = _package_archive(tmp_path / "input", "windows", "x86_64")
    Path(f"{archive}.sha256").write_text(f"{'1' * 64}  {archive.name}\n", encoding="ascii")
    output = tmp_path / "release"
    completed = subprocess.run(
        _prepare_command(package, archive, output, "windows-x86_64-standalone-max"),
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 2
    assert "sidecar does not exactly match" in completed.stderr
    assert not output.exists()


def _fake_mcp_server(path: Path) -> None:
    path.write_text(
        """#!/usr/bin/env python3
import json
import sys

for line in sys.stdin:
    request = json.loads(line)
    if "id" not in request:
        continue
    if request["method"] == "initialize":
        value = {
            "protocolVersion": "2025-11-25",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "fake", "version": "1"},
        }
        response = {"jsonrpc": "2.0", "id": request["id"], "result": value}
    else:
        name = request["params"]["name"]
        values = {
            "score_create": {"score_id": 1, "part_ids": [1]},
            "score_set_formal_plan": {"ok": True},
            "score_insert_note": {"ok": True},
            "create_timbre_profile": {"profile_id": 1, "success": True},
            "map_timbre_parameter": {"success": True},
            "create_mix_graph": {"graph_id": 1, "success": True},
            "set_channel_level": {"success": True},
            "project_validate": {"valid": True},
            "project_plan_to_ableton": {
                "success": True,
                "plan_id": 9,
                "one_shot": True,
                "planned_mutations": [{"sequence": 0}],
                "snapshot": {"target": "fake"},
            },
            "project_apply_ableton_plan": {
                "success": True,
                "validation_record": {"schema_version": 1, "complete": False},
            },
        }
        text = json.dumps(values[name], separators=(",", ":"))
        result = {"content": [{"type": "text", "text": text}], "isError": False}
        response = {"jsonrpc": "2.0", "id": request["id"], "result": result}
    print(json.dumps(response, separators=(",", ":")), flush=True)
""",
        encoding="utf-8",
    )
    path.chmod(0o755)


def _fake_host_provenance(root: Path) -> None:
    remote = root / "Sunny"
    remote.mkdir()
    for name in ("__init__.py", "bridge_contract.json", "handler.py", "server.py", "surface.py"):
        (remote / name).write_text(f"synthetic fixture: {name}\n", encoding="utf-8")
    (root / "Log.txt").write_text("synthetic Live log fixture\n", encoding="utf-8")


def _live_command(server: Path, output: Path, *, apply: bool = False) -> list[str]:
    command = [
        sys.executable,
        str(LIVE_RUNNER),
        "--server",
        str(server),
        "--remote-script-root",
        str(server.parent / "Sunny"),
        "--live-log",
        str(server.parent / "Log.txt"),
        "--output-root",
        str(output),
        "--live-edition",
        "Suite",
        "--operating-system",
        "Named OS",
        "--architecture",
        "x86_64",
        "--remote-script-revision",
        "0123456789abcdef",
        "--timeout",
        "5",
    ]
    if apply:
        command.extend(
            [
                "--max-version",
                "9.0.5",
                "--max-for-live-version",
                "9.0.5",
                "--license-state",
                "licensed",
                "--cleanup-step",
                "Delete the validation track after evidence retention.",
                "--apply",
            ]
        )
    return command


def test_named_live_runner_defaults_to_retained_plan_without_apply(tmp_path: Path) -> None:
    """The Live helper stops after retaining a target-observing dry-run by default."""
    server = tmp_path / "fake-mcp"
    _fake_mcp_server(server)
    _fake_host_provenance(tmp_path)
    output = tmp_path / "plan"
    completed = subprocess.run(
        _live_command(server, output), check=False, capture_output=True, text=True
    )
    assert completed.returncode == 0, completed.stderr
    transcript = json.loads((output / "json-rpc-transcript.json").read_text(encoding="utf-8"))
    assert transcript["mode"] == "plan_only"
    assert transcript["status"] == "plan_retained_without_mutation"
    methods = [
        event["request"]["params"]["name"]
        for event in transcript["events"]
        if "request" in event and event["request"]["method"] == "tools/call"
    ]
    assert methods[-1] == "project_plan_to_ableton"
    assert "project_apply_ableton_plan" not in methods
    assert (output / "dry-run-plan.json").is_file()
    assert (output / "ableton-live-log-before.txt").is_file()
    assert (output / "ableton-live-log-after.txt").is_file()
    assert len(transcript["server_file"]["sha256"]) == 64
    assert len(transcript["installed_remote_script"]["tree_sha256"]) == 64
    assert not (output / "ableton-validation-record.json").exists()


def test_named_live_runner_retains_explicit_guarded_apply_record(tmp_path: Path) -> None:
    """Explicit apply retains the returned validation record beside the transcript."""
    server = tmp_path / "fake-mcp"
    _fake_mcp_server(server)
    _fake_host_provenance(tmp_path)
    output = tmp_path / "apply"
    completed = subprocess.run(
        _live_command(server, output, apply=True),
        check=False,
        capture_output=True,
        text=True,
    )
    assert completed.returncode == 0, completed.stderr
    transcript = json.loads((output / "json-rpc-transcript.json").read_text(encoding="utf-8"))
    assert transcript["mode"] == "guarded_apply"
    assert transcript["status"] == "guarded_apply_returned_validation_record"
    record = json.loads((output / "ableton-validation-record.json").read_text(encoding="utf-8"))
    assert record == {"schema_version": 1, "complete": False}
