"""Authenticate packaged verifier bytes before importing any release delegate."""

from __future__ import annotations

import importlib.util
import json
import subprocess

import pytest
from test_release_tool import PROJECT, _hash, _make_release


def load(path, name):
    """Import a reviewed fixture helper, without running its command line."""
    specification = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(specification)
    module.__file__ = str(path)
    exec(compile(path.read_bytes(), str(path), "exec"), module.__dict__)
    return module


@pytest.fixture
def packaged(tmp_path):
    """Pair the actual operator code with independent synthetic OCI test bytes."""
    producer = load(PROJECT / "tools/release.py", "qualification_test_producer")
    root = tmp_path / "paired release"
    _make_release(
        root,
        producer,
        operator_files={
            "release.py": (PROJECT / "tools/release.py").read_bytes(),
            "live_qualification/verify_artifacts.py": (
                PROJECT / "tools/live_qualification/verify_artifacts.py"
            ).read_bytes(),
        },
    )
    kit = load(
        root / "operator/live_qualification/verify_artifacts.py", "packaged_qualification_verifier"
    )
    return root, kit, _hash((root / "release.json").read_bytes())


def test_packaged_verification_uses_actual_oci_contract_without_effects(packaged, monkeypatch):
    """An authenticated delegate verifies bytes without invoking Docker or native Live."""
    root, kit, checksum = packaged

    def forbidden(*args, **kwargs):
        pytest.fail("Offline identity verification tried to run a process")

    monkeypatch.setattr(subprocess, "run", forbidden)
    result = kit.verify_paired_release(root, checksum)
    assert result["release_manifest_sha256"] == checksum
    assert result["source_revision"] == "4" * 40
    assert result["configuration_schema_version"] == 1
    assert result["host_executed"] is False and result["toolkit_in_release"] is True
    assert result["qualification"] == {
        "native_live": "pending",
        "production_configuration": "pending",
        "registry_publication": "not_authorized",
    }
    assert not list(root.rglob("__pycache__")), "Read-only verification changed the release"


@pytest.mark.parametrize("component", ["release.py", "live_qualification/verify_artifacts.py"])
def test_modified_operator_is_rejected_before_delegate_import(packaged, component):
    """A substituted helper cannot execute its top-level marker before authentication."""
    root, kit, checksum = packaged
    marker = root.parent / "delegate-executed"
    target = root / "operator" / component
    target.write_text("from pathlib import Path\nPath(" + repr(str(marker)) + ").touch()\n")
    with pytest.raises(ValueError, match="Operator bytes differ"):
        kit.verify_paired_release(root, checksum)
    assert not marker.exists()


@pytest.mark.parametrize("checksum", [None, "", "A" * 64, "0" * 64])
def test_separately_retained_manifest_checksum_is_required(packaged, checksum):
    """No manifest-provided checksum can substitute for the independent trust input."""
    root, kit, _ = packaged
    with pytest.raises(ValueError, match="checksum|SHA256"):
        kit.verify_paired_release(root, checksum)


def test_duplicate_operator_inventory_is_rejected_before_import(packaged):
    """Even an authenticated duplicate inventory cannot select executable helper bytes."""
    root, kit, _ = packaged
    path = root / "release.json"
    manifest = json.loads(path.read_text())
    original = next(row for row in manifest["files"] if row["path"] == "operator/release.py")
    manifest["files"].append(dict(original))
    path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError, match="exact packaged operator"):
        kit.verify_paired_release(root, _hash(path.read_bytes()))


def test_symlinked_matching_delegate_is_not_imported(packaged):
    """Identical content does not make a redirected executable a physical release unit."""
    root, kit, checksum = packaged
    delegate = root / "operator/release.py"
    outside = root.parent / "redirected verifier.py"
    delegate.rename(outside)
    delegate.symlink_to(outside)
    with pytest.raises(ValueError, match="Operator bytes differ"):
        kit.verify_paired_release(root, checksum)
