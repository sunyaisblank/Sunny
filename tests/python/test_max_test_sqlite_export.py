"""Tests for the thin official max-test SQLite row extractor and native handoff."""

from __future__ import annotations

import importlib.util
import json
import os
import sqlite3
import subprocess
import sys
from pathlib import Path
from types import ModuleType

import pytest


def _exporter_module() -> ModuleType:
    path = (
        Path(__file__).parents[2]
        / "max-package"
        / "misc"
        / "validation"
        / "export-max-test-sqlite.py"
    )
    specification = importlib.util.spec_from_file_location("sunny_max_test_exporter", path)
    assert specification is not None and specification.loader is not None
    module = importlib.util.module_from_spec(specification)
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        specification.loader.exec_module(module)
    finally:
        sys.dont_write_bytecode = previous
    return module


EXPORTER = _exporter_module()
REPOSITORY = Path(__file__).parents[2]
MANIFEST = REPOSITORY / "max-package/misc/validation/max-test-harness.json"
OBSERVATION = REPOSITORY / "max-package/misc/validation/max-validation-observation.example.json"


def _assertion_names() -> list[str]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    names: set[str] = set()
    for section in ("artifact_assertions", "check_assertions"):
        for mapping in manifest[section]:
            names.update(mapping["required_assertions"])
    return sorted(names)


def _create_database(
    path: Path,
    *,
    outcome_overrides: dict[str, str] | None = None,
    finished: bool = True,
    include_assertion_finish: bool = True,
) -> None:
    outcome_overrides = outcome_overrides or {}
    path.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(path) as database:
        database.execute(
            "CREATE TABLE tests ("
            "test_id INTEGER PRIMARY KEY, test_name VARCHAR(512), "
            "test_start DATETIME, test_finish DATETIME)"
        )
        finish_value: str | int = "2026-08-31 10:00:02" if finished else 0
        database.execute(
            "INSERT INTO tests(test_id, test_name, test_start, test_finish) VALUES (?, ?, ?, ?)",
            (42, "sunny-runtime-smoke.maxtest", "2026-08-31 10:00:00", finish_value),
        )
        finish_column = ", assertion_finish DATETIME" if include_assertion_finish else ""
        database.execute(
            "CREATE TABLE assertions ("
            "assertion_id INTEGER PRIMARY KEY, test_id_ext INTEGER, "
            "assertion_name VARCHAR(512), assertion_value VARCHAR(512), "
            "assertion_data VARCHAR(512), assertion_start DATETIME"
            f"{finish_column}, assertion_tags VARCHAR(512))"
        )
        if include_assertion_finish:
            for assertion_id, name in enumerate(_assertion_names(), start=100):
                database.execute(
                    "INSERT INTO assertions("
                    "assertion_id, test_id_ext, assertion_name, assertion_value, "
                    "assertion_finish) VALUES (?, ?, ?, ?, ?)",
                    (
                        assertion_id,
                        42,
                        name,
                        outcome_overrides.get(name, "Pass"),
                        "2026-08-31 10:00:01",
                    ),
                )
        database.execute(
            "CREATE TABLE logs ("
            "log_id INTEGER PRIMARY KEY, test_id_ext INTEGER, text VARCHAR(512), "
            "timestamp DATETIME)"
        )


def test_sqlite_export_is_an_exact_raw_projection(tmp_path: Path) -> None:
    """Preserve exact row identities, timestamps, names, outcomes, and database digest."""
    database = tmp_path / "results.db3"
    _create_database(database, outcome_overrides={"sunny-lfo-output": "Fail"})

    result = EXPORTER.extract_max_test_run(MANIFEST, database, 42)
    assert result["schema_version"] == 1
    assert len(result["database_sha256"]) == 64
    assert result["test"] == {
        "id": 42,
        "name": "sunny-runtime-smoke.maxtest",
        "started_at": "2026-08-31 10:00:00",
        "finished_at": "2026-08-31 10:00:02",
    }
    assert len(result["assertions"]) == 64
    assert [row["id"] for row in result["assertions"]] == list(range(100, 164))
    outcomes = {row["name"]: row["outcome"] for row in result["assertions"]}
    assert outcomes["sunny-lfo-output"] == "Fail"
    assert set(outcomes) == set(_assertion_names())


def test_sqlite_export_rejects_unfinished_unknown_and_incomplete_rows(tmp_path: Path) -> None:
    """Decline incomplete tests, open outcome vocabularies, and schema loss."""
    unfinished = tmp_path / "unfinished.db3"
    _create_database(unfinished, finished=False)
    with pytest.raises(ValueError, match="test_finish must be non-empty text"):
        EXPORTER.extract_max_test_run(MANIFEST, unfinished, 42)

    unknown = tmp_path / "unknown.db3"
    _create_database(unknown, outcome_overrides={"sunny-lfo-output": "Maybe"})
    with pytest.raises(ValueError, match="unrecognized outcome"):
        EXPORTER.extract_max_test_run(MANIFEST, unknown, 42)

    incomplete = tmp_path / "incomplete.db3"
    _create_database(incomplete, include_assertion_finish=False)
    with pytest.raises(ValueError, match="missing columns from assertions"):
        EXPORTER.extract_max_test_run(MANIFEST, incomplete, 42)


def test_sqlite_export_requires_one_closed_quiescent_database(tmp_path: Path) -> None:
    """Reject active journal state, an absent run identity, and a symlink database leaf."""
    database = tmp_path / "results.db3"
    _create_database(database)
    Path(f"{database}-wal").write_bytes(b"active")
    with pytest.raises(ValueError, match="not a closed quiescent file"):
        EXPORTER.extract_max_test_run(MANIFEST, database, 42)

    Path(f"{database}-wal").unlink()
    with pytest.raises(ValueError, match="no unique test_id"):
        EXPORTER.extract_max_test_run(MANIFEST, database, 99)

    link = tmp_path / "linked.db3"
    try:
        link.symlink_to(database)
    except OSError:
        return
    with pytest.raises(ValueError, match="non-symlink regular file"):
        EXPORTER.extract_max_test_run(MANIFEST, link, 42)


def test_native_cli_rehashes_and_maps_the_exact_database(tmp_path: Path) -> None:
    """Bind raw extraction to the retained DB and let C++ decide the fourteen outcomes."""
    evidence_root = tmp_path / "evidence"
    database = evidence_root / "max-test/results.db3"
    _create_database(database)
    result = EXPORTER.extract_max_test_run(MANIFEST, database, 42)
    result_path = tmp_path / "result.json"
    result_path.write_text(json.dumps(result), encoding="utf-8")

    tool = Path(
        os.environ.get("SUNNY_MAX_EVIDENCE_TOOL", str(REPOSITORY / ".bin/sunny-max-evidence"))
    )
    applied = subprocess.run(
        [
            str(tool),
            "apply-max-test",
            str(MANIFEST),
            str(result_path),
            str(OBSERVATION),
            str(evidence_root),
            "max-test/results.db3",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    observation = json.loads(applied.stdout)
    automated = {
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
        "standalone_transport_identity",
    }
    for check in observation["checks"]:
        if check["check"] in automated:
            assert check["outcome"] == "passed"
            assert check["evidence_relative_path"] == "max-test/results.db3"
        elif check["check"] == "live_transport_discontinuities":
            assert check["outcome"] == "not_applicable"
        else:
            assert check["outcome"] == "not_run"
    assert all(
        artifact["discovered"] and artifact["instantiated"] for artifact in observation["artifacts"]
    )

    sidecar = Path(f"{database}-wal")
    sidecar.write_bytes(b"active")
    non_quiescent = subprocess.run(
        [
            str(tool),
            "apply-max-test",
            str(MANIFEST),
            str(result_path),
            str(OBSERVATION),
            str(evidence_root),
            "max-test/results.db3",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert non_quiescent.returncode == 4
    assert "max_test_database_not_quiescent" in non_quiescent.stderr
    sidecar.unlink()

    with sqlite3.connect(database) as changed:
        changed.execute("UPDATE assertions SET assertion_value = 'Fail' WHERE assertion_id = 100")
    rejected = subprocess.run(
        [
            str(tool),
            "apply-max-test",
            str(MANIFEST),
            str(result_path),
            str(OBSERVATION),
            str(evidence_root),
            "max-test/results.db3",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert rejected.returncode == 5
    assert "database_digest_mismatch: database_sha256" in rejected.stderr
