"""End-to-end tests for native six-cell Max release-matrix assembly."""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
from copy import deepcopy
from pathlib import Path

REPOSITORY = Path(__file__).parents[2]
MANIFEST = REPOSITORY / "max-package/misc/validation/max-release-matrix.json"
EXAMPLE = REPOSITORY / "max-package/misc/validation/max-validation-record.example.json"
RECORD_PATHS = (
    "records/macos-x86_64-standalone-max.json",
    "records/macos-x86_64-max-for-live.json",
    "records/macos-arm64-standalone-max.json",
    "records/macos-arm64-max-for-live.json",
    "records/windows-x86_64-standalone-max.json",
    "records/windows-x86_64-max-for-live.json",
)


def _digest(label: str) -> str:
    return hashlib.sha256(label.encode("utf-8")).hexdigest()


def _record(
    operating_system: str, architecture: str, host_kind: str, target: str
) -> dict[str, object]:
    record = deepcopy(json.loads(EXAMPLE.read_text(encoding="utf-8")))
    record["source_revision"] = "0123456789abcdef0123456789abcdef01234567"
    record["package_archive_sha256"] = _digest(f"archive:{target}")
    record["observed_at_utc"] = "2026-08-31T10:00:00Z"
    record["harness"] = "Sunny release validation harness 1"
    environment = record["environment"]
    assert isinstance(environment, dict)
    environment.update(
        {
            "host_kind": host_kind,
            "max_version": "9.0.5",
            "live_version": "12.3.1" if host_kind == "max_for_live" else None,
            "max_for_live_version": "9.0.5" if host_kind == "max_for_live" else None,
            "operating_system": operating_system,
            "architecture": architecture,
            "audio_driver": "Core Audio" if operating_system == "macos" else "ASIO fixture",
            "sample_rate": 48000.0,
            "io_vector_size": 512,
            "signal_vector_size": 64,
            "overdrive": True,
            "scheduler_in_audio_interrupt": True,
        }
    )
    artifacts = record["artifacts"]
    assert isinstance(artifacts, list)
    for artifact in artifacts:
        assert isinstance(artifact, dict)
        name = artifact["object_name"]
        artifact["package_relative_path"] = (
            f"externals/{name}.mxo/Contents/MacOS/{name}"
            if operating_system == "macos"
            else f"externals/{name}.mxe64"
        )
        artifact["binary_sha256"] = _digest(f"binary:{target}:{name}")
        artifact["discovered"] = True
        artifact["instantiated"] = True
    checks = record["checks"]
    assert isinstance(checks, list)
    for check in checks:
        assert isinstance(check, dict)
        name = str(check["check"])
        applicable = not (
            (name == "live_transport_discontinuities" and host_kind != "max_for_live")
            or (name == "standalone_transport_identity" and host_kind != "standalone_max")
        )
        check["outcome"] = "passed" if applicable else "not_applicable"
        check["summary"] = (
            "Observed expected host behavior"
            if applicable
            else "Check belongs to the other host kind"
        )
        check["evidence_relative_path"] = f"evidence/{name}.json" if applicable else None
        check["evidence_sha256"] = (
            _digest(f"evidence:{target}:{host_kind}:{name}") if applicable else None
        )
    record["complete"] = True
    return record


def _write_records(root: Path) -> None:
    specifications = (
        ("macos", "x86_64", "standalone_max", "macos-x86_64"),
        ("macos", "x86_64", "max_for_live", "macos-x86_64"),
        ("macos", "arm64", "standalone_max", "macos-arm64"),
        ("macos", "arm64", "max_for_live", "macos-arm64"),
        ("windows", "x86_64", "standalone_max", "windows-x86_64"),
        ("windows", "x86_64", "max_for_live", "windows-x86_64"),
    )
    for relative, specification in zip(RECORD_PATHS, specifications, strict=True):
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(_record(*specification), indent=2) + "\n", encoding="utf-8")


def _tool() -> Path:
    return Path(
        os.environ.get("SUNNY_MAX_EVIDENCE_TOOL", str(REPOSITORY / ".bin/sunny-max-evidence"))
    )


def test_native_cli_assembles_and_reverifies_all_six_release_cells(tmp_path: Path) -> None:
    """Bind one coherent complete record for every supported target/host product cell."""
    _write_records(tmp_path)
    assembled = subprocess.run(
        [str(_tool()), "assemble-matrix", str(MANIFEST), str(tmp_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    matrix = json.loads(assembled.stdout)
    assert matrix["complete"] is True
    assert len(matrix["cells"]) == 6
    assert all(cell["record"]["complete"] for cell in matrix["cells"])

    matrix_path = tmp_path / "release-matrix.json"
    matrix_path.write_text(assembled.stdout, encoding="utf-8")
    verified = subprocess.run(
        [str(_tool()), "verify-matrix", str(matrix_path), str(MANIFEST), str(tmp_path)],
        check=True,
        capture_output=True,
        text=True,
    )
    assert verified.stdout == "verified\n"

    first = tmp_path / RECORD_PATHS[0]
    first.write_text(first.read_text(encoding="utf-8") + "\n", encoding="utf-8")
    rejected = subprocess.run(
        [str(_tool()), "verify-matrix", str(matrix_path), str(MANIFEST), str(tmp_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    assert rejected.returncode == 5
    assert "record_digest_mismatch" in rejected.stderr


def test_native_cli_rejects_cross_record_revision_drift(tmp_path: Path) -> None:
    """Do not aggregate individually complete records from different source revisions."""
    _write_records(tmp_path)
    path = tmp_path / RECORD_PATHS[3]
    changed = json.loads(path.read_text(encoding="utf-8"))
    changed["source_revision"] = "1123456789abcdef0123456789abcdef01234567"
    path.write_text(json.dumps(changed, indent=2) + "\n", encoding="utf-8")

    rejected = subprocess.run(
        [str(_tool()), "assemble-matrix", str(MANIFEST), str(tmp_path)],
        check=False,
        capture_output=True,
        text=True,
    )
    assert rejected.returncode == 5
    assert "revision_mismatch" in rejected.stderr
