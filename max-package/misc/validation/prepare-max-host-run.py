#!/usr/bin/env python3
"""Create one non-overwriting, evidence-empty Sunny Max host-run workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import zipfile
from datetime import datetime
from pathlib import Path, PurePosixPath

OBJECTS = ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events")
CELL_NAMES = {
    ("macos", "x86_64", "standalone_max"): "macos-x86_64-standalone-max",
    ("macos", "x86_64", "max_for_live"): "macos-x86_64-max-for-live",
    ("macos", "arm64", "standalone_max"): "macos-arm64-standalone-max",
    ("macos", "arm64", "max_for_live"): "macos-arm64-max-for-live",
    ("windows", "x86_64", "standalone_max"): "windows-x86_64-standalone-max",
    ("windows", "x86_64", "max_for_live"): "windows-x86_64-max-for-live",
}


def _read_json(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"JSON root is not an object: {path}")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for block in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _regular_file(path: Path, subject: str) -> Path:
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"{subject} is missing, a symlink, or not a regular file: {path}")
    return path.resolve(strict=True)


def _validate_source_revision(value: str) -> None:
    if not re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", value) or set(value) == {"0"}:
        raise ValueError("source revision must be one nonzero lowercase 40- or 64-hex digest")


def _validate_timestamp(value: str) -> None:
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", value):
        raise ValueError("observed timestamp must use exact YYYY-MM-DDTHH:MM:SSZ form")
    try:
        datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise ValueError("observed timestamp is not a valid UTC calendar time") from error


def _parse_boolean(value: str) -> bool:
    if value == "true":
        return True
    if value == "false":
        return False
    raise argparse.ArgumentTypeError("expected true or false")


def _validate_archive(archive: Path, package_root: Path, expected_name: str) -> str:
    archive = _regular_file(archive, "package archive")
    if archive.name != expected_name:
        raise ValueError(f"archive name must be {expected_name}, observed {archive.name}")
    sidecar = _regular_file(Path(f"{archive}.sha256"), "package archive digest sidecar")
    digest = _sha256(archive)
    expected_sidecar = f"{digest}  {archive.name}\n"
    if sidecar.read_text(encoding="ascii") != expected_sidecar:
        raise ValueError("archive digest sidecar does not exactly match the archive bytes")

    if package_root.is_symlink() or not package_root.is_dir():
        raise ValueError(f"package root is missing, a symlink, or not a directory: {package_root}")
    package_root = package_root.resolve(strict=True)
    package_files: dict[str, Path] = {}
    for path in package_root.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"extracted package contains a symlink: {path}")
        if path.is_file():
            package_files[f"Sunny/{path.relative_to(package_root).as_posix()}"] = path

    try:
        with zipfile.ZipFile(archive) as zipped:
            if zipped.testzip() is not None:
                raise ValueError("package archive failed its ZIP CRC check")
            entries = [entry for entry in zipped.infolist() if not entry.is_dir()]
            names = [entry.filename for entry in entries]
            if len(names) != len(set(names)):
                raise ValueError("package archive contains duplicate paths")
            for entry in entries:
                path = PurePosixPath(entry.filename)
                unix_type = (entry.external_attr >> 16) & 0o170000
                if (
                    path.is_absolute()
                    or "\\" in entry.filename
                    or any(component in {"", ".", ".."} for component in path.parts)
                    or unix_type == 0o120000
                ):
                    raise ValueError(f"unsafe package archive entry: {entry.filename}")
            if set(names) != set(package_files):
                missing = sorted(set(package_files) - set(names))
                unexpected = sorted(set(names) - set(package_files))
                raise ValueError(
                    f"archive/package topology differs: missing={missing}, unexpected={unexpected}"
                )
            for entry in entries:
                if zipped.read(entry) != package_files[entry.filename].read_bytes():
                    raise ValueError(f"archive/package bytes differ: {entry.filename}")
    except zipfile.BadZipFile as error:
        raise ValueError(f"invalid package archive: {archive}") from error
    return digest


def _release_cells(release_manifest: dict[str, object]) -> dict[str, dict[str, str]]:
    cells = release_manifest.get("cells")
    if not isinstance(cells, list) or len(cells) != len(CELL_NAMES):
        raise ValueError("release manifest does not contain the six required cells")
    result: dict[str, dict[str, str]] = {}
    for cell in cells:
        if not isinstance(cell, dict) or set(cell) != {
            "operating_system",
            "architecture",
            "host_kind",
            "record_relative_path",
        }:
            raise ValueError("release manifest cell shape is not closed")
        identity = (
            cell.get("operating_system"),
            cell.get("architecture"),
            cell.get("host_kind"),
        )
        if identity not in CELL_NAMES:
            raise ValueError(f"unsupported release cell identity: {identity}")
        name = CELL_NAMES[identity]
        if name in result or not isinstance(cell.get("record_relative_path"), str):
            raise ValueError("duplicate or malformed release cell")
        result[name] = {key: str(value) for key, value in cell.items()}
    if set(result) != set(CELL_NAMES.values()):
        raise ValueError("release manifest cell set is incomplete")
    return result


def _artifact_path(operating_system: str, object_name: str) -> str:
    if operating_system == "windows":
        return f"externals/{object_name}.mxe64"
    return f"externals/{object_name}.mxo/Contents/MacOS/{object_name}"


def _write_json(path: Path, value: object) -> None:
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def _scaffold(arguments: argparse.Namespace) -> Path:
    script_directory = Path(__file__).resolve().parent
    plan_path = (arguments.plan or script_directory / "max-host-run-plan.json").resolve(strict=True)
    plan = _read_json(plan_path)
    if (
        set(plan)
        != {
            "schema_version",
            "plan_name",
            "sunny_version",
            "release_manifest",
            "observation_template",
            "baseline_environment",
            "official_contracts",
            "max_test",
            "max_for_live_assertions",
            "evidence_directories",
            "retained_artifacts",
            "normalized_residual_result",
            "residual_checks",
            "max_for_live_transport_scenarios",
            "named_live_run",
        }
        or plan.get("schema_version") != 1
        or plan.get("plan_name") != "sunny-max-named-host-run"
        or plan.get("sunny_version") != "0.4.0"
    ):
        raise ValueError("host-run plan identity or closed schema is invalid")

    release_path = script_directory / str(plan["release_manifest"])
    template_path = script_directory / str(plan["observation_template"])
    release_manifest = _read_json(release_path)
    cells = _release_cells(release_manifest)
    if arguments.cell not in cells:
        raise ValueError(f"unknown release cell: {arguments.cell}")
    cell = cells[arguments.cell]
    host_kind = cell["host_kind"]
    live_host = host_kind == "max_for_live"
    live_assertions = plan.get("max_for_live_assertions")
    if (
        not isinstance(live_assertions, dict)
        or live_assertions.get("device_kind") != "max_audio_effect"
    ):
        raise ValueError("Max for Live device kind is not the pinned Audio Effect contract")
    if (live_host and not (arguments.live_version and arguments.max_for_live_version)) or (
        not live_host and (arguments.live_version or arguments.max_for_live_version)
    ):
        raise ValueError(
            "Max for Live cells require both --live-version and --max-for-live-version; "
            "standalone cells require neither"
        )

    _validate_source_revision(arguments.source_revision)
    _validate_timestamp(arguments.observed_at_utc)
    if not arguments.max_version.strip() or not arguments.audio_driver.strip():
        raise ValueError("Max version and audio driver must be non-empty observed strings")
    if (
        arguments.sample_rate <= 0
        or arguments.io_vector_size < 1
        or arguments.signal_vector_size < 1
    ):
        raise ValueError("sample rate and vector sizes must be positive")

    expected_archive = (
        f"Sunny-{plan['sunny_version']}-{cell['operating_system']}-{cell['architecture']}.zip"
    )
    archive_digest = _validate_archive(arguments.archive, arguments.package_root, expected_archive)
    package_root = arguments.package_root.resolve(strict=True)
    for object_name in OBJECTS:
        _regular_file(
            package_root / _artifact_path(cell["operating_system"], object_name),
            f"{object_name} package binary",
        )

    output_root = arguments.output_root.resolve()
    cell_root = output_root / "cells" / arguments.cell
    if cell_root.exists():
        raise ValueError(f"refusing to overwrite an existing cell workspace: {cell_root}")
    evidence_root = cell_root / "evidence"
    evidence_directories = plan.get("evidence_directories")
    if (
        not isinstance(evidence_directories, list)
        or not evidence_directories
        or any(not isinstance(value, str) or not value for value in evidence_directories)
    ):
        raise ValueError("host-run evidence directory set is invalid")

    observation = _read_json(template_path)
    observation.update(
        {
            "sunny_version": plan["sunny_version"],
            "package_version": plan["sunny_version"],
            "source_revision": arguments.source_revision,
            "observed_at_utc": arguments.observed_at_utc,
            "harness": (
                "sunny-max-host-plan-v1+max-for-live-operator"
                if live_host
                else "sunny-max-host-plan-v1+cycling74-max-test"
            ),
            "environment": {
                "host_kind": host_kind,
                "max_version": arguments.max_version,
                "live_version": arguments.live_version,
                "max_for_live_version": arguments.max_for_live_version,
                "operating_system": cell["operating_system"],
                "architecture": cell["architecture"],
                "audio_driver": arguments.audio_driver,
                "sample_rate": float(arguments.sample_rate),
                "io_vector_size": arguments.io_vector_size,
                "signal_vector_size": arguments.signal_vector_size,
                "overdrive": arguments.overdrive,
                "scheduler_in_audio_interrupt": arguments.scheduler_in_audio_interrupt,
            },
            "artifacts": [
                {
                    "object_name": object_name,
                    "package_relative_path": _artifact_path(cell["operating_system"], object_name),
                    "discovered": False,
                    "instantiated": False,
                }
                for object_name in OBJECTS
            ],
        }
    )
    for check in observation.get("checks", []):
        check["outcome"] = "not_run"
        check["evidence_relative_path"] = None
        check["summary"] = "Not run; this scaffold contains no named-host observation."
        if check.get("check") == "live_transport_discontinuities" and not live_host:
            check["outcome"] = "not_applicable"
            check["summary"] = "Standalone Max does not use Live transport discontinuities."
        if check.get("check") == "standalone_transport_identity" and live_host:
            check["outcome"] = "not_applicable"
            check["summary"] = "Max for Live must instead exercise Live transport discontinuities."

    record_path = output_root / cell["record_relative_path"]
    normalized_result = cell_root / "max-test-result.json"
    host_run_result = evidence_root / "host-run-result.json"
    database = evidence_root / "max-test" / "results.db3"
    observation_path = cell_root / "observation.json"
    command_plan: dict[str, object] = {
        "schema_version": 1,
        "cell": arguments.cell,
        "cell_identity": {
            "operating_system": cell["operating_system"],
            "architecture": cell["architecture"],
            "host_kind": host_kind,
        },
        "source_revision": arguments.source_revision,
        "archive": str(arguments.archive.resolve(strict=True)),
        "archive_sha256": archive_digest,
        "archive_sidecar": str(Path(f"{arguments.archive.resolve(strict=True)}.sha256")),
        "package_root": str(package_root),
        "observation": str(observation_path),
        "evidence_root": str(evidence_root),
        "record": str(record_path),
        "max_test_applicable": not live_host,
        "m4l_device_source": (
            {
                "device_kind": live_assertions["device_kind"],
                "output_root": str(cell_root / "device-source"),
                "prepare": [
                    "python3",
                    str(package_root / "misc/validation/prepare-m4l-device-source.py"),
                    str(package_root),
                    "<installed-pinned-max-test-package-root>",
                    str(package_root / "misc/validation/max-test-harness.json"),
                    str(cell_root / "device-source"),
                ],
                "save_from_named_host_to": str(
                    evidence_root / "live-transport-discontinuities/sunny-validation-device.amxd"
                ),
                "scenario_result_template": str(cell_root / "scenario-results.template.json"),
                "authority": (
                    "The generated .maxpat tree is editable source only. A named Live/Max for "
                    "Live build must save and freeze the retained .amxd."
                ),
            }
            if live_host
            else None
        ),
        "commands_after_host_exit": {
            "extract_max_test": (
                [
                    "python3",
                    str(package_root / "misc/validation/export-max-test-sqlite.py"),
                    str(package_root / "misc/validation/max-test-harness.json"),
                    str(database),
                    "<positive-test-id>",
                ]
                if not live_host
                else None
            ),
            "normalized_result_output": str(normalized_result) if not live_host else None,
            "export_m4l_assertions": (
                [
                    "python3",
                    str(package_root / "misc/validation/export-m4l-assertions.py"),
                    str(evidence_root / "live-transport-discontinuities/max-test-harness.json"),
                    str(
                        evidence_root
                        / "live-transport-discontinuities/sunny-validation-device.amxd"
                    ),
                    str(evidence_root / "live-transport-discontinuities/host-console.txt"),
                    "<exact-run-id-from-console-markers>",
                ]
                if live_host
                else None
            ),
            "m4l_assertion_result_output": (
                str(evidence_root / "live-transport-discontinuities/shared-assertions.json")
                if live_host
                else None
            ),
            "apply_max_test": (
                [
                    "sunny-max-evidence",
                    "apply-max-test",
                    str(package_root / "misc/validation/max-test-harness.json"),
                    str(normalized_result),
                    str(observation_path),
                    str(evidence_root),
                    "max-test/results.db3",
                ]
                if not live_host
                else None
            ),
            "index_host_run": [
                "python3",
                str(package_root / "misc/validation/index-max-host-run.py"),
                str(package_root / "misc/validation/max-host-run-plan.json"),
                str(observation_path),
                str(evidence_root),
            ],
            "host_run_result_output": str(host_run_result),
            "apply_host_run": [
                "sunny-max-evidence",
                "apply-host-run",
                str(package_root / "misc/validation/max-host-run-plan.json"),
                str(host_run_result),
                str(observation_path),
                str(evidence_root),
                "host-run-result.json",
            ],
            "verify_host_run": [
                "sunny-max-evidence",
                "verify-host-run",
                str(package_root / "misc/validation/max-host-run-plan.json"),
                str(host_run_result),
                str(evidence_root),
                "host-run-result.json",
            ],
            "assemble_record": [
                "sunny-max-evidence",
                "assemble",
                str(observation_path),
                str(arguments.archive.resolve(strict=True)),
                str(package_root),
                str(evidence_root),
            ],
            "record_output": str(record_path),
            "verify_record": [
                "sunny-max-evidence",
                "verify",
                str(record_path),
                str(arguments.archive.resolve(strict=True)),
                str(package_root),
                str(evidence_root),
            ],
        },
        "warning": (
            "The editable M4L source and console exporter are preparation only; all applicable "
            "checks remain not_run until the named-host-saved .amxd and device-context output "
            "supply evidence."
            if live_host
            else "The max-test database can update only its 14 mapped checks; residual checks "
            "remain not_run."
        ),
    }

    (output_root / "records").mkdir(parents=True, exist_ok=True)
    for directory in evidence_directories:
        (evidence_root / directory).mkdir(parents=True, exist_ok=False)
    if live_host:
        shutil.copyfile(
            script_directory / "max-test-harness.json",
            evidence_root / "live-transport-discontinuities/max-test-harness.json",
        )
        _write_json(
            cell_root / "scenario-results.template.json",
            {
                "schema_version": 1,
                "live_set_name": "<replace-with-exact-live-set-name>",
                "device_sha256": "<replace-with-saved-amxd-sha256>",
                "assertion_run_id": "<replace-with-exact-marked-run-id>",
                "scenarios": [
                    {
                        "name": scenario["name"],
                        "completed": False,
                        "scheduled_event_count": None,
                        "expected_fire_count": None,
                        "observed_fire_count": None,
                        "duplicate_fire_count": None,
                        "stale_output_count": None,
                        "callback_healthy": None,
                        "final_reserved_slots": None,
                        "final_retained_events": None,
                        "observation": "<replace-with-observed-discontinuity-account>",
                    }
                    for scenario in plan["max_for_live_transport_scenarios"]
                ],
            },
        )
    _write_json(observation_path, observation)
    _write_json(cell_root / "operator-inputs.json", command_plan)
    return cell_root


def main() -> None:
    """Parse one cell request and create its evidence-empty workspace."""
    parser = argparse.ArgumentParser(
        description=(
            "Verify one exact package/archive pair and create an evidence-empty host-run cell. "
            "The command never marks a host check passed."
        )
    )
    parser.add_argument("--plan", type=Path)
    parser.add_argument("--cell", choices=sorted(CELL_NAMES.values()), required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--source-revision", required=True)
    parser.add_argument("--observed-at-utc", required=True)
    parser.add_argument("--max-version", required=True)
    parser.add_argument("--live-version")
    parser.add_argument("--max-for-live-version")
    parser.add_argument("--audio-driver", required=True)
    parser.add_argument("--sample-rate", type=float, default=48000.0)
    parser.add_argument("--io-vector-size", type=int, default=512)
    parser.add_argument("--signal-vector-size", type=int, default=64)
    parser.add_argument("--overdrive", type=_parse_boolean, default=True)
    parser.add_argument("--scheduler-in-audio-interrupt", type=_parse_boolean, default=True)
    arguments = parser.parse_args()
    try:
        cell_root = _scaffold(arguments)
    except (OSError, UnicodeError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(2, f"prepare-max-host-run: {error}\n")
    print(f"prepared evidence-empty host cell: {cell_root}")
    print("No host check was marked passed.")


if __name__ == "__main__":
    main()
