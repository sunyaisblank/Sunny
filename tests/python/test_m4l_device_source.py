"""Tests for the hostless Max-for-Live source and assertion-projection boundary."""

from __future__ import annotations

import json
import runpy
from pathlib import Path
from types import SimpleNamespace

import pytest

REPOSITORY = Path(__file__).resolve().parents[2]
VALIDATION = REPOSITORY / "max-package/misc/validation"
MANIFEST = VALIDATION / "max-test-harness.json"


def _module(path: Path) -> SimpleNamespace:
    return SimpleNamespace(**runpy.run_path(str(path)))


EXPORTER = _module(VALIDATION / "export-m4l-assertions.py")
GENERATOR = _module(VALIDATION / "prepare-m4l-device-source.py")


def _protocol_console(run_id: str, assertions: list[str]) -> str:
    return (
        "retained Live console prefix\n"
        f"SUNNY_M4L_ASSERTIONS_BEGIN|{run_id}\n"
        + "".join(f"SUNNY_M4L_ASSERTION|{run_id}|{name}|Pass\n" for name in assertions)
        + f"SUNNY_M4L_ASSERTIONS_END|{run_id}|{len(assertions)}\n"
        "retained Live console suffix\n"
    )


def test_m4l_console_export_binds_device_console_contract_and_exact_assertions(
    tmp_path: Path,
) -> None:
    """The exporter projects one exact protocol session and derives no check verdicts."""
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    expected = GENERATOR._assertion_names(manifest)
    assert expected == EXPORTER._assertion_names(manifest)
    device = tmp_path / "sunny-validation-device.amxd"
    console = tmp_path / "host-console.txt"
    device.write_bytes(b"synthetic device parser fixture\n")
    console.write_text(_protocol_console("run-123", expected), encoding="utf-8")

    result = EXPORTER.export_assertions(MANIFEST, device, console, "run-123")
    assert set(result) == {
        "schema_version",
        "harness_manifest_sha256",
        "device_sha256",
        "console_sha256",
        "run_id",
        "assertions",
    }
    assert result["run_id"] == "run-123"
    assert [entry["name"] for entry in result["assertions"]] == expected
    assert {entry["outcome"] for entry in result["assertions"]} == {"Pass"}
    assert not any("check" in key or "passed" in key for key in result)


def test_m4l_console_export_rejects_duplicate_or_unmarked_assertions(tmp_path: Path) -> None:
    """A stale, duplicated, or out-of-session assertion cannot enter normalized evidence."""
    expected = GENERATOR._assertion_names(json.loads(MANIFEST.read_text(encoding="utf-8")))
    device = tmp_path / "sunny-validation-device.amxd"
    console = tmp_path / "host-console.txt"
    device.write_bytes(b"synthetic device parser fixture\n")
    console.write_text(
        _protocol_console("run-duplicate", expected).replace(
            f"SUNNY_M4L_ASSERTIONS_END|run-duplicate|{len(expected)}",
            f"SUNNY_M4L_ASSERTION|run-duplicate|{expected[0]}|Pass\n"
            f"SUNNY_M4L_ASSERTIONS_END|run-duplicate|{len(expected)}",
        ),
        encoding="utf-8",
    )
    with pytest.raises(ValueError, match="duplicate assertion"):
        EXPORTER.export_assertions(MANIFEST, device, console, "run-duplicate")


def test_m4l_source_transform_removes_runner_private_assertions_and_adds_context() -> None:
    """The generated device source uses the marked JS protocol, not max-test's private #T unit."""
    smoke = json.loads(
        (REPOSITORY / "max-package/patchers/sunny-runtime-smoke.maxtest.maxpat").read_text(
            encoding="utf-8"
        )
    )
    GENERATOR._transform(smoke, main=True)
    patcher = smoke["patcher"]
    texts = [entry["box"].get("text", "") for entry in patcher["boxes"]]
    assert "live.thisdevice" in texts
    assert "plugout~" in texts
    assert "dac~" not in texts
    assert "js sunny.m4l.assert.js @start" in texts
    assert "js sunny.m4l.assert.js @finish" in texts
    assert not any(
        text.startswith(("test.assert", "test.terminate", "sunny.assert", "CheckConsoleClear"))
        for text in texts
    )
    assert "test.sample~" in texts
    assert any(text.startswith("test.equals ") for text in texts)
    load_edges = [
        entry["patchline"]
        for entry in patcher["lines"]
        if entry["patchline"]["source"] == ["load", 0]
    ]
    assert sorted(edge["order"] for edge in load_edges) == [0, 1]
    assert not any(
        edge["patchline"]["source"] == ["dsp-on", 0]
        or edge["patchline"]["destination"] == ["dac", 0]
        for edge in patcher["lines"]
    )

    expected = GENERATOR._assertion_names(json.loads(MANIFEST.read_text(encoding="utf-8")))
    javascript = GENERATOR._javascript(expected)
    assert javascript.count("SUNNY_M4L_ASSERTION|") == 1
    assert all(json.dumps(name) in javascript for name in expected)
