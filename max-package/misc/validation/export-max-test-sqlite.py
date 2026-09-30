#!/usr/bin/env python3
"""Project one quiescent max-test SQLite run into Sunny's closed raw-result JSON."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sqlite3
import stat
import sys
from pathlib import Path
from typing import Any

_IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*", re.ASCII)
_DATABASE_FIELDS = {
    "format",
    "test_table",
    "test_id_column",
    "test_name_column",
    "test_start_column",
    "test_finish_column",
    "assertion_table",
    "assertion_id_column",
    "assertion_test_id_column",
    "assertion_name_column",
    "assertion_outcome_column",
    "assertion_finish_column",
    "admitted_outcomes",
}


def _regular_file(path: Path, label: str) -> Path:
    """Resolve one present regular file while rejecting a symlink leaf."""
    try:
        status = path.lstat()
    except OSError as error:
        raise ValueError(f"unavailable {label}: {path}") from error
    if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
        raise ValueError(f"{label} must be a non-symlink regular file: {path}")
    return path.resolve(strict=True)


def _sha256(path: Path) -> str:
    """Hash one file and reject a size or write-time change during the read."""
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(64 * 1024), b""):
            digest.update(block)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError(f"database changed while being hashed: {path}")
    return digest.hexdigest()


def _sqlite_sidecars(database: Path) -> tuple[Path, ...]:
    return tuple(Path(f"{database}{suffix}") for suffix in ("-journal", "-wal", "-shm"))


def _require_quiescent(database: Path) -> None:
    present = [path for path in _sqlite_sidecars(database) if path.exists()]
    if present:
        names = ", ".join(path.name for path in present)
        raise ValueError(f"max-test database is not a closed quiescent file: {names}")


def _identifier(value: Any, field: str) -> str:
    if not isinstance(value, str) or _IDENTIFIER.fullmatch(value) is None:
        raise ValueError(f"invalid SQLite identifier in harness manifest: {field}")
    return f'"{value}"'


def _positive_integer(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"max-test {field} must be a positive integer")
    return value


def _text(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value or len(value) > 512:
        raise ValueError(f"max-test {field} must be non-empty text")
    if any(ord(character) < 0x20 or ord(character) == 0x7F for character in value):
        raise ValueError(f"max-test {field} contains a control character")
    return value


def _database_contract(manifest_path: Path) -> dict[str, Any]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        contract = manifest["harness"]["result_database"]
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise ValueError("invalid max-test harness manifest") from error
    if not isinstance(contract, dict) or set(contract) != _DATABASE_FIELDS:
        raise ValueError("max-test harness has an open or incomplete database contract")
    if contract["format"] != "sqlite3" or contract["admitted_outcomes"] != ["Pass", "Fail"]:
        raise ValueError("unsupported max-test database format or outcome algebra")
    for field in _DATABASE_FIELDS - {"format", "admitted_outcomes"}:
        _identifier(contract[field], field)
    return contract


def _require_columns(connection: sqlite3.Connection, table: str, required: set[str]) -> None:
    observed = {
        row[1] for row in connection.execute(f"PRAGMA table_info({_identifier(table, table)})")
    }
    missing = required - observed
    if missing:
        raise ValueError(f"max-test database is missing columns from {table}: {sorted(missing)}")


def extract_max_test_run(manifest_path: Path, database_path: Path, test_id: int) -> dict[str, Any]:
    """Extract raw rows only; C++ owns assertion/check semantics and observation mutation."""
    manifest = _regular_file(manifest_path, "harness manifest")
    database = _regular_file(database_path, "max-test database")
    test_id = _positive_integer(test_id, "test_id")
    contract = _database_contract(manifest)

    _require_quiescent(database)
    digest_before = _sha256(database)
    uri = f"{database.as_uri()}?mode=ro"
    with sqlite3.connect(uri, uri=True) as connection:
        connection.execute("PRAGMA query_only = ON")
        if connection.execute("PRAGMA quick_check").fetchall() != [("ok",)]:
            raise ValueError("max-test database failed SQLite quick_check")

        test_table = contract["test_table"]
        assertion_table = contract["assertion_table"]
        test_columns = {
            contract["test_id_column"],
            contract["test_name_column"],
            contract["test_start_column"],
            contract["test_finish_column"],
        }
        assertion_columns = {
            contract["assertion_id_column"],
            contract["assertion_test_id_column"],
            contract["assertion_name_column"],
            contract["assertion_outcome_column"],
            contract["assertion_finish_column"],
        }
        _require_columns(connection, test_table, test_columns)
        _require_columns(connection, assertion_table, assertion_columns)

        test_fields = [
            contract["test_id_column"],
            contract["test_name_column"],
            contract["test_start_column"],
            contract["test_finish_column"],
        ]
        selected_tests = connection.execute(
            f"SELECT {', '.join(_identifier(field, field) for field in test_fields)} "
            f"FROM {_identifier(test_table, 'test_table')} "
            f"WHERE {_identifier(contract['test_id_column'], 'test_id_column')} = ?",
            (test_id,),
        ).fetchall()
        if len(selected_tests) != 1:
            raise ValueError(f"max-test database has no unique test_id {test_id}")
        selected_test = selected_tests[0]

        assertion_fields = [
            contract["assertion_id_column"],
            contract["assertion_name_column"],
            contract["assertion_outcome_column"],
            contract["assertion_finish_column"],
        ]
        assertion_test_id = _identifier(
            contract["assertion_test_id_column"], "assertion_test_id_column"
        )
        assertion_id = _identifier(contract["assertion_id_column"], "assertion_id_column")
        assertion_rows = connection.execute(
            f"SELECT {', '.join(_identifier(field, field) for field in assertion_fields)} "
            f"FROM {_identifier(assertion_table, 'assertion_table')} "
            f"WHERE {assertion_test_id} = ? ORDER BY {assertion_id}",
            (test_id,),
        ).fetchall()

    _require_quiescent(database)
    digest_after = _sha256(database)
    if digest_before != digest_after:
        raise ValueError("max-test database changed while rows were extracted")

    admitted = set(contract["admitted_outcomes"])
    assertions: list[dict[str, Any]] = []
    for assertion_id, name, outcome, finished_at in assertion_rows:
        if outcome not in admitted:
            raise ValueError(f"max-test assertion has an unrecognized outcome: {outcome!r}")
        assertions.append(
            {
                "id": _positive_integer(assertion_id, "assertion_id"),
                "name": _text(name, "assertion_name"),
                "outcome": outcome,
                "finished_at": _text(finished_at, "assertion_finish"),
            }
        )

    return {
        "schema_version": 1,
        "database_sha256": digest_after,
        "test": {
            "id": _positive_integer(selected_test[0], "test_id"),
            "name": _text(selected_test[1], "test_name"),
            "started_at": _text(selected_test[2], "test_start"),
            "finished_at": _text(selected_test[3], "test_finish"),
        },
        "assertions": assertions,
    }


def main() -> None:
    """Extract one selected run and write normalized JSON to stdout."""
    parser = argparse.ArgumentParser(
        description="Extract one raw Cycling '74 max-test SQLite run for native Sunny validation."
    )
    parser.add_argument("manifest", type=Path)
    parser.add_argument("database", type=Path)
    parser.add_argument("test_id", type=int)
    arguments = parser.parse_args()
    try:
        result = extract_max_test_run(arguments.manifest, arguments.database, arguments.test_id)
    except (OSError, sqlite3.Error, ValueError) as error:
        parser.exit(3, f"export-max-test-sqlite: {error}\n")
    json.dump(result, sys.stdout, indent=2, ensure_ascii=False, allow_nan=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
