#!/usr/bin/env python3
"""Hash residual Max host artifacts without interpreting their measurements."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import sys
from pathlib import Path
from typing import Any


def _read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def _regular_file(path: Path, subject: str) -> os.stat_result:
    status = path.lstat()
    if stat.S_ISLNK(status.st_mode) or not stat.S_ISREG(status.st_mode):
        raise ValueError(f"{subject} must be a direct non-symlink regular file: {path}")
    return status


def _stable_sha256(path: Path, subject: str) -> str:
    before = _regular_file(path, subject)
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    after = _regular_file(path, subject)
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


def _contract(plan: dict[str, Any]) -> dict[str, Any]:
    contract = plan.get("normalized_residual_result")
    if (
        plan.get("schema_version") != 1
        or plan.get("plan_name") != "sunny-max-named-host-run"
        or plan.get("sunny_version") != "0.4.0"
        or not isinstance(contract, dict)
        or set(contract)
        != {
            "schema_version",
            "indexer",
            "evidence_relative_path",
            "file_roles",
            "max_for_live_file_roles",
        }
        or contract.get("schema_version") != 1
        or contract.get("indexer") != "index-max-host-run.py"
        or contract.get("evidence_relative_path") != "host-run-result.json"
        or not isinstance(contract.get("file_roles"), list)
        or len(contract["file_roles"]) != 12
        or not isinstance(contract.get("max_for_live_file_roles"), list)
        or len(contract["max_for_live_file_roles"]) != 5
    ):
        raise ValueError("host-run plan residual-result contract is invalid")
    roles: set[str] = set()
    paths: set[str] = set()
    for entry in contract["file_roles"] + contract["max_for_live_file_roles"]:
        if (
            not isinstance(entry, dict)
            or set(entry) != {"role", "relative_path"}
            or not isinstance(entry["role"], str)
            or not entry["role"]
            or not isinstance(entry["relative_path"], str)
            or not entry["relative_path"]
            or entry["role"] in roles
            or entry["relative_path"] in paths
        ):
            raise ValueError("host-run plan contains duplicate or malformed residual file roles")
        relative = Path(entry["relative_path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("host-run plan contains an unsafe residual evidence path")
        roles.add(entry["role"])
        paths.add(entry["relative_path"])
    return contract


def index_host_run(plan_path: Path, observation_path: Path, evidence_root: Path) -> dict[str, Any]:
    """Return a content index; native code remains responsible for all residual outcomes."""
    plan_path = plan_path.resolve(strict=True)
    observation_path = observation_path.resolve(strict=True)
    evidence_root = evidence_root.resolve(strict=True)
    plan = _read_json(plan_path)
    contract = _contract(plan)
    observation = _read_json(observation_path)
    required_observation = {
        "schema_version",
        "sunny_version",
        "package_version",
        "source_revision",
        "observed_at_utc",
        "harness",
        "environment",
        "artifacts",
        "checks",
    }
    if (
        set(observation) != required_observation
        or observation.get("schema_version") != 1
        or observation.get("sunny_version") != plan.get("sunny_version")
        or not isinstance(observation.get("package_version"), str)
        or not isinstance(observation.get("source_revision"), str)
        or not isinstance(observation.get("observed_at_utc"), str)
        or not isinstance(observation.get("environment"), dict)
        or observation["environment"].get("host_kind") not in {"standalone_max", "max_for_live"}
    ):
        raise ValueError("observation provenance is incomplete or inconsistent with the plan")

    file_contracts = list(contract["file_roles"])
    if observation["environment"].get("host_kind") == "max_for_live":
        file_contracts.extend(contract["max_for_live_file_roles"])
    files: list[dict[str, str]] = []
    for entry in file_contracts:
        relative_path = entry["relative_path"]
        path = evidence_root / relative_path
        files.append(
            {
                "role": entry["role"],
                "relative_path": relative_path,
                "sha256": _stable_sha256(path, entry["role"]),
            }
        )

    return {
        "schema_version": contract["schema_version"],
        "plan_sha256": _stable_sha256(plan_path, "host-run plan"),
        "sunny_version": observation["sunny_version"],
        "package_version": observation["package_version"],
        "source_revision": observation["source_revision"],
        "observed_at_utc": observation["observed_at_utc"],
        "environment": observation["environment"],
        "files": files,
    }


def main() -> None:
    """Parse paths and write one outcome-free residual content index to stdout."""
    parser = argparse.ArgumentParser(
        description=(
            "Hash the twelve shared and conditional M4L artifacts and write a schema-1 index. "
            "This command never decides or reports a host-check outcome."
        )
    )
    parser.add_argument("plan", type=Path)
    parser.add_argument("observation", type=Path)
    parser.add_argument("evidence_root", type=Path)
    arguments = parser.parse_args()
    try:
        value = index_host_run(arguments.plan, arguments.observation, arguments.evidence_root)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.exit(2, f"index-max-host-run: {error}\n")
    json.dump(value, sys.stdout, indent=2, ensure_ascii=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
