#!/usr/bin/env python3
"""Export one marked Max-for-Live assertion session without deriving check outcomes."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import stat
import sys
from pathlib import Path
from typing import Any

MAX_TEST_HARNESS_SHA256 = "1d26434e9c3a00b5604070c9bd950755365c2a82a7f642fd205fb17d95db39bc"
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
PROTOCOL = re.compile(
    r"SUNNY_M4L_ASSERTIONS_(?P<boundary>BEGIN|END)\|(?P<run>[A-Za-z0-9._:-]{1,128})"
    r"(?:\|(?P<count>[0-9]{1,4}))?"
    r"|SUNNY_M4L_ASSERTION\|(?P<assert_run>[A-Za-z0-9._:-]{1,128})"
    r"\|(?P<name>[A-Za-z0-9:~._-]{1,512})\|(?P<outcome>Pass|Fail)"
)


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


def _stable_sha256(path: Path, subject: str) -> str:
    path = _regular_file(path, subject)
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    if (
        before.st_dev,
        before.st_ino,
        before.st_size,
        before.st_mtime_ns,
    ) != (
        after.st_dev,
        after.st_ino,
        after.st_size,
        after.st_mtime_ns,
    ):
        raise ValueError(f"{subject} changed while it was being hashed: {path}")
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
        or len(artifacts) != 5
        or not isinstance(checks, list)
        or len(checks) != 19
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
        if (
            not isinstance(required, list)
            or not required
            or any(not isinstance(name, str) or not name or "|" in name for name in required)
        ):
            raise ValueError("max-test assertion mapping is malformed")
        names.update(required)
    result = sorted(names)
    if len(result) != 64:
        raise ValueError("shared M4L assertion contract must contain exactly 64 names")
    return result


def export_assertions(
    manifest_path: Path,
    saved_device: Path,
    console_path: Path,
    run_id: str,
) -> dict[str, Any]:
    """Return the exact marked assertion facts for one device-context console session."""
    if not re.fullmatch(r"[A-Za-z0-9._:-]{1,128}", run_id):
        raise ValueError("run id must contain 1-128 protocol-safe characters")
    manifest_digest = _stable_sha256(manifest_path, "max-test harness manifest")
    if manifest_digest != MAX_TEST_HARNESS_SHA256:
        raise ValueError("max-test harness manifest digest is not the compiled pin")
    expected = _assertion_names(_read_json(manifest_path))
    device_digest = _stable_sha256(saved_device, "host-saved Max for Live device")
    console_digest = _stable_sha256(console_path, "Max for Live console transcript")
    if Path(saved_device).suffix.lower() != ".amxd":
        raise ValueError("host-saved Max for Live device must use the .amxd suffix")
    console = Path(console_path).read_text(encoding="utf-8")
    if not console:
        raise ValueError("Max for Live console transcript is empty")

    begin: list[int] = []
    end: list[tuple[int, int]] = []
    assertions: list[tuple[int, str, str]] = []
    for match in PROTOCOL.finditer(console):
        marker_run = match.group("run")
        assertion_run = match.group("assert_run")
        if marker_run == run_id:
            if match.group("boundary") == "BEGIN":
                if match.group("count") is not None:
                    raise ValueError("BEGIN marker must not carry an assertion count")
                begin.append(match.start())
            else:
                encoded_count = match.group("count")
                if encoded_count is None:
                    raise ValueError("END marker must carry the assertion count")
                end.append((match.start(), int(encoded_count)))
        if assertion_run == run_id:
            assertions.append(
                (match.start(), str(match.group("name")), str(match.group("outcome")))
            )
    if len(begin) != 1 or len(end) != 1 or begin[0] >= end[0][0]:
        raise ValueError("target run must have exactly one ordered BEGIN/END marker pair")
    if end[0][1] != len(expected):
        raise ValueError("target run END marker has the wrong assertion count")
    if any(position <= begin[0] or position >= end[0][0] for position, _, _ in assertions):
        raise ValueError("target run assertion lies outside its marked session")

    outcomes: dict[str, str] = {}
    for _, name, outcome in assertions:
        if name in outcomes:
            raise ValueError(f"target run contains duplicate assertion: {name}")
        outcomes[name] = outcome
    if sorted(outcomes) != expected:
        missing = sorted(set(expected) - set(outcomes))
        unexpected = sorted(set(outcomes) - set(expected))
        raise ValueError(
            f"target run assertion set differs: missing={missing}, unexpected={unexpected}"
        )
    return {
        "schema_version": 1,
        "harness_manifest_sha256": manifest_digest,
        "device_sha256": device_digest,
        "console_sha256": console_digest,
        "run_id": run_id,
        "assertions": [{"name": name, "outcome": outcomes[name]} for name in expected],
    }


def main() -> None:
    """Parse one marked session and write its outcome-free assertion projection to stdout."""
    parser = argparse.ArgumentParser(
        description=(
            "Export one exact Sunny M4L assertion protocol session. The command preserves "
            "Pass/Fail facts but never derives a host-check outcome."
        )
    )
    parser.add_argument("manifest", type=Path)
    parser.add_argument("saved_device", type=Path)
    parser.add_argument("console", type=Path)
    parser.add_argument("run_id")
    arguments = parser.parse_args()
    try:
        result = export_assertions(
            arguments.manifest, arguments.saved_device, arguments.console, arguments.run_id
        )
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as error:
        parser.exit(2, f"export-m4l-assertions: {error}\n")
    json.dump(result, sys.stdout, indent=2, ensure_ascii=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
