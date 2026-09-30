#!/usr/bin/env python3
"""Build an editable M4L validation-device source tree from the pinned standalone smoke."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import stat
from pathlib import Path
from typing import Any

MAX_TEST_HARNESS_SHA256 = "1d26434e9c3a00b5604070c9bd950755365c2a82a7f642fd205fb17d95db39bc"
CHECK_CONSOLE_CLEAR_SHA256 = "fe8084df5a1350380f18d9299714865d6a68d2e28bb3cfbe5fbf1398f2df3028"
SHARED_CHECKS = (
    "package_discovery",
    "public_class_surface",
    "signal_topology",
    "dsp_setup",
    "perform_callback",
    "finite_signal_output",
    "disconnected_processing",
    "reconnect_continuity",
    "control_dispatch",
    "itm_schedule_fire",
    "itm_equal_tick_ordering",
    "itm_clear_reassign",
    "release_velocity_formatting",
)
PATCHER_RENAMES = {
    "sunny.assert-host-status": "sunny.m4l.assert-host-status",
    "sunny.assert-event-status": "sunny.m4l.assert-event-status",
    "sunny.assert-event-list": "sunny.m4l.assert-event-list",
    "sunny.assert-host-lifecycle": "sunny.m4l.assert-host-lifecycle",
}


def _read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def _regular_file(path: Path, subject: str) -> Path:
    status = path.lstat()
    if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
        raise ValueError(f"{subject} must be a direct non-symlink regular file: {path}")
    return path.resolve(strict=True)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _assertion_names(manifest: dict[str, Any]) -> list[str]:
    harness = manifest.get("harness")
    artifacts = manifest.get("artifact_assertions")
    checks = manifest.get("check_assertions")
    if (
        manifest.get("schema_version") != 1
        or not isinstance(harness, dict)
        or harness.get("revision") != "8c5d833d4b1e454238ced7c866c34acedb69cc07"
        or harness.get("host_kind") != "standalone_max"
        or not isinstance(artifacts, list)
        or not isinstance(checks, list)
    ):
        raise ValueError("max-test assertion contract identity is invalid")
    selected = {
        entry.get("check"): entry
        for entry in checks
        if isinstance(entry, dict) and entry.get("check") in SHARED_CHECKS
    }
    if tuple(name for name in SHARED_CHECKS if name in selected) != SHARED_CHECKS:
        raise ValueError("max-test assertion contract omits a shared M4L check")
    names: set[str] = set()
    for entry in [*artifacts, *(selected[name] for name in SHARED_CHECKS)]:
        required = entry.get("required_assertions") if isinstance(entry, dict) else None
        if not isinstance(required, list) or not required:
            raise ValueError("max-test assertion mapping is malformed")
        for name in required:
            if not isinstance(name, str) or not name or "|" in name:
                raise ValueError("max-test assertion name is not protocol-safe")
            names.add(name)
    result = sorted(names)
    if len(result) != 64:
        raise ValueError("shared M4L assertion contract must contain exactly 64 names")
    return result


def _replace_text(text: str, *, console: bool = False) -> str:
    if text.startswith("test.assert "):
        return "js sunny.m4l.assert.js " + text.removeprefix("test.assert ")
    if text == "test.terminate":
        return "js sunny.m4l.assert.js @finish"
    if console and text.startswith("test.log "):
        return "print sunny-m4l-console-error"
    if text.startswith("CheckConsoleClear "):
        return "sunny.m4l.assert-console-clear " + text.removeprefix("CheckConsoleClear ")
    for source, target in PATCHER_RENAMES.items():
        if text == source or text.startswith(source + " "):
            return target + text[len(source) :]
    return text


def _transform(document: dict[str, Any], *, main: bool = False, console: bool = False) -> None:
    patcher = document.get("patcher")
    if not isinstance(patcher, dict) or not isinstance(patcher.get("boxes"), list):
        raise ValueError("source patcher is malformed")
    for encoded in patcher["boxes"]:
        box = encoded.get("box") if isinstance(encoded, dict) else None
        if not isinstance(box, dict):
            raise ValueError("source patcher contains a malformed box")
        text = box.get("text")
        if isinstance(text, str):
            box["text"] = _replace_text(text, console=console)
    if not main:
        return
    if not isinstance(patcher.get("lines"), list):
        raise ValueError("source patcher line set is malformed")
    boxes_by_id = {encoded["box"].get("id"): encoded["box"] for encoded in patcher["boxes"]}
    dsp_start = boxes_by_id.get("dsp-on")
    audio_sink = boxes_by_id.get("dac")
    if (
        not isinstance(dsp_start, dict)
        or dsp_start.get("maxclass") != "message"
        or dsp_start.get("text") != "1"
        or not isinstance(audio_sink, dict)
        or audio_sink.get("maxclass") != "newobj"
        or audio_sink.get("text") != "dac~"
    ):
        raise ValueError("standalone DSP-start topology differs from the pinned smoke")
    dsp_start["text"] = "0"
    audio_sink["text"] = "plugout~"
    dsp_start_edges = []
    retained_lines = []
    for encoded in patcher["lines"]:
        line = encoded.get("patchline") if isinstance(encoded, dict) else None
        if not isinstance(line, dict):
            raise ValueError("source patcher contains a malformed line")
        if line.get("source") == ["dsp-on", 0] or line.get("destination") == ["dac", 0]:
            dsp_start_edges.append((line.get("source"), line.get("destination")))
            continue
        retained_lines.append(encoded)
    if dsp_start_edges != [(["dsp-on", 0], ["dac", 0])]:
        raise ValueError("standalone dac~ participation differs from the pinned smoke")
    patcher["lines"] = retained_lines
    for encoded in patcher["boxes"]:
        box = encoded["box"]
        if box.get("id") == "title":
            box["text"] = (
                "Sunny Max for Live Audio Effect shared-runtime validation source. "
                "Save from the named Live/Max build as .amxd; this .maxpat is not host evidence."
            )
        if box.get("id") == "boundary":
            box["text"] = (
                "This source preserves the 64 pinned shared assertions. Four residual measurements "
                "and six Live discontinuity scenarios remain separate retained evidence."
            )
    patcher["boxes"].extend(
        [
            {
                "box": {
                    "id": "m4l-device-context",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [""],
                    "patching_rect": [25.0, 55.0, 96.0, 22.0],
                    "text": "live.thisdevice",
                }
            },
            {
                "box": {
                    "id": "m4l-assertion-start",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [130.0, 55.0, 194.0, 22.0],
                    "text": "js sunny.m4l.assert.js @start",
                }
            },
        ]
    )
    load_lines = 0
    for encoded in patcher["lines"]:
        line = encoded.get("patchline") if isinstance(encoded, dict) else None
        if isinstance(line, dict) and line.get("source") == ["load", 0]:
            line["order"] = int(line.get("order", 0)) + 1
            load_lines += 1
    if load_lines != 1:
        raise ValueError("source smoke must have one exact top-level loadbang edge")
    patcher["lines"].append(
        {
            "patchline": {
                "source": ["load", 0],
                "destination": ["m4l-assertion-start", 0],
                "order": 0,
            }
        }
    )


def _javascript(expected: list[str]) -> str:
    encoded = ",\n    ".join(json.dumps(name) for name in expected)
    return f"""/* Generated by prepare-m4l-device-source.py from the pinned Sunny assertion map. */
autowatch = 0;
inlets = 1;
outlets = 0;

var mode = jsarguments.length > 1 ? String(jsarguments[1]) : "";
var expected = [
    {encoded}
];
var state = new Global("sunny_m4l_validation_state_v1");

function key(index) {{
    return "assertion_" + String(index);
}}

function index_of_name(name) {{
    for (var index = 0; index < expected.length; ++index) {{
        if (expected[index] === name) return index;
    }}
    return -1;
}}

function start_session() {{
    state.run_id = String(new Date().getTime()) + "-" +
        String(Math.floor(Math.random() * 1000000000));
    state.started = 1;
    for (var index = 0; index < expected.length; ++index) state[key(index)] = "Missing";
    post("SUNNY_M4L_ASSERTIONS_BEGIN|" + state.run_id + "\\n");
}}

function record(outcome) {{
    if (state.started !== 1) return;
    var index = index_of_name(mode);
    if (index < 0) {{
        post("SUNNY_M4L_PROTOCOL_ERROR|unknown-assertion|" + mode + "\\n");
        return;
    }}
    state[key(index)] = outcome;
}}

function finish_session() {{
    if (state.started !== 1) {{
        post("SUNNY_M4L_PROTOCOL_ERROR|finish-without-start\\n");
        return;
    }}
    for (var index = 0; index < expected.length; ++index) {{
        var outcome = state[key(index)] === "Pass" ? "Pass" : "Fail";
        post("SUNNY_M4L_ASSERTION|" + state.run_id + "|" + expected[index] + "|" + outcome + "\\n");
    }}
    post("SUNNY_M4L_ASSERTIONS_END|" + state.run_id + "|" + String(expected.length) + "\\n");
    state.started = 0;
}}

function bang() {{
    if (mode === "@start") start_session();
    else if (mode === "@finish") finish_session();
    else record("Fail");
}}

function msg_int(value) {{
    if (mode.charAt(0) !== "@") record(value === 1 ? "Pass" : "Fail");
}}

function msg_float(value) {{
    if (mode.charAt(0) !== "@") record(value === 1.0 ? "Pass" : "Fail");
}}

function list() {{
    if (mode.charAt(0) !== "@") record("Fail");
}}

function anything() {{
    if (mode.charAt(0) !== "@") record("Fail");
}}
"""


def prepare(
    package_root: Path,
    max_test_root: Path,
    manifest_path: Path,
    output_root: Path,
) -> Path:
    """Create one non-overwriting, provenance-inventoried editable device source tree."""
    package_root = package_root.resolve(strict=True)
    max_test_root = max_test_root.resolve(strict=True)
    manifest_path = _regular_file(manifest_path, "max-test harness manifest")
    if _sha256(manifest_path) != MAX_TEST_HARNESS_SHA256:
        raise ValueError("max-test harness manifest digest is not the compiled pin")
    manifest = _read_json(manifest_path)
    expected = _assertion_names(manifest)
    console_source = _regular_file(
        max_test_root / "patchers/lib/CheckConsoleClear.maxpat", "pinned CheckConsoleClear"
    )
    if _sha256(console_source) != CHECK_CONSOLE_CLEAR_SHA256:
        raise ValueError("CheckConsoleClear bytes differ from the pinned upstream revision")

    output_root = output_root.resolve()
    if output_root.exists():
        raise ValueError(f"refusing to overwrite an existing device-source tree: {output_root}")
    temporary = output_root.with_name(f".{output_root.name}.tmp-{os.getpid()}")
    if temporary.exists():
        raise ValueError(f"temporary output path already exists: {temporary}")
    patchers = temporary / "patchers"
    javascript = temporary / "javascript"
    patchers.mkdir(parents=True)
    javascript.mkdir()
    inputs: dict[str, str] = {
        "max-test-harness.json": _sha256(manifest_path),
        "max-test/patchers/lib/CheckConsoleClear.maxpat": _sha256(console_source),
    }
    try:
        main_source = _regular_file(
            package_root / "patchers/sunny-runtime-smoke.maxtest.maxpat", "Sunny runtime smoke"
        )
        main_document = _read_json(main_source)
        inputs["patchers/sunny-runtime-smoke.maxtest.maxpat"] = _sha256(main_source)
        _transform(main_document, main=True)
        (patchers / "sunny-m4l-validation-device.maxpat").write_text(
            json.dumps(main_document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )

        for source_name, target_name in PATCHER_RENAMES.items():
            source = _regular_file(package_root / f"patchers/{source_name}.maxpat", source_name)
            document = _read_json(source)
            inputs[f"patchers/{source_name}.maxpat"] = _sha256(source)
            _transform(document)
            (patchers / f"{target_name}.maxpat").write_text(
                json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
            )

        console_document = _read_json(console_source)
        _transform(console_document, console=True)
        (patchers / "sunny.m4l.assert-console-clear.maxpat").write_text(
            json.dumps(console_document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        (javascript / "sunny.m4l.assert.js").write_text(_javascript(expected), encoding="utf-8")
        shutil.copyfile(manifest_path, temporary / "max-test-harness.json")

        generated: dict[str, str] = {}
        for path in sorted(temporary.rglob("*")):
            if path.is_file():
                generated[path.relative_to(temporary).as_posix()] = _sha256(path)
        inventory = {
            "schema_version": 1,
            "authority": (
                "Editable source only for a Max for Live Audio Effect. Replace the blank device "
                "patcher content, save and freeze with the named Live/Max build, and retain that "
                "host-saved .amxd as evidence."
            ),
            "device_kind": "max_audio_effect",
            "audio_topology": {
                "standalone_dsp_start_removed": True,
                "device_output": "unconnected plugout~ (silence)",
            },
            "max_test_dependency": {
                "revision": "8c5d833d4b1e454238ced7c866c34acedb69cc07",
                "package_version": "1.2.1",
                "required_objects": ["test.sample~", "test.equals"],
            },
            "assertion_count": len(expected),
            "shared_checks": list(SHARED_CHECKS),
            "inputs": inputs,
            "generated": generated,
        }
        (temporary / "source-inventory.json").write_text(
            json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        temporary.replace(output_root)
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise
    return output_root


def main() -> None:
    """Parse source roots and generate a deterministic editable M4L device source tree."""
    parser = argparse.ArgumentParser(
        description=(
            "Transform the pinned standalone smoke into editable Max for Live device source. "
            "The generated .maxpat is never execution evidence or a substitute for a saved .amxd."
        )
    )
    parser.add_argument("package_root", type=Path)
    parser.add_argument("max_test_root", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output_root", type=Path)
    arguments = parser.parse_args()
    try:
        output = prepare(
            arguments.package_root,
            arguments.max_test_root,
            arguments.manifest,
            arguments.output_root,
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.exit(2, f"prepare-m4l-device-source: {error}\n")
    print(output)


if __name__ == "__main__":
    main()
