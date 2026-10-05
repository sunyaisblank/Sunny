"""A transport ZIP preserves the full same-image release without substituting a checkout kit."""

from __future__ import annotations

import importlib.util
import json
import zipfile

import pytest
from test_release_tool import PROJECT, _hash, _make_release


def load(path, name):
    """Read a reviewed helper without running its command line."""
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(), str(path), "exec"), module.__dict__)
    return module


@pytest.fixture
def producer(monkeypatch):
    """Use real release/packager code with an independent synthetic OCI fixture."""
    monkeypatch.syspath_prepend(str(PROJECT / "tools"))
    return load(PROJECT / "tools/export_live_qualification.py", "qualification_zip_test")


def paired(tmp_path, producer, omit=None):
    """Include actual operator bytes in both physical release and synthetic image rootfs."""
    root = tmp_path / "same image release"
    operators = {"release.py": (PROJECT / "tools/release.py").read_bytes()}
    if omit != "runbook":
        operators["README.md"] = (PROJECT / "README.md").read_bytes()
    if omit != "kit":
        for name in (
            "common.py",
            "host_runner.py",
            "verify_artifacts.py",
            "configuration.json",
            "obligations.json",
            "SunnyHostProbe/probe.py",
        ):
            if name == omit:
                continue
            operators["live_qualification/" + name] = (
                PROJECT / "tools/live_qualification" / name
            ).read_bytes()
    _make_release(root, producer.release, operator_files=operators)
    return root


def test_zip_contains_exact_full_paired_release_and_inventory(tmp_path, producer):
    """Extraction verifies independently and retains the exact source/OCI/operator bytes."""
    root = paired(tmp_path, producer)
    archive = tmp_path / "Sunny release with spaces.zip"
    report = producer.package(root, archive)
    assert report["host_executed"] is False
    assert report["release_manifest_sha256"] == _hash((root / "release.json").read_bytes())
    assert report["sha256"] == _hash(archive.read_bytes())
    before = {
        p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob("*") if p.is_file()
    }
    with zipfile.ZipFile(archive) as zipped:
        assert set(zipped.namelist()) == {"release/" + path for path in before}
        assert all(zipped.read("release/" + path) == data for path, data in before.items())
        extracted = tmp_path / "extracted"
        zipped.extractall(extracted)
    checked = producer.release.verify_release(
        extracted / "release", report["release_manifest_sha256"]
    )
    assert checked["image"]["local_immutable_id"] == report["image_local_immutable_id"]
    assert not list(root.rglob("__pycache__"))


@pytest.mark.parametrize("missing", ["kit", "runbook", "configuration.json", "obligations.json"])
def test_old_image_cannot_borrow_the_present_checkout_kit(tmp_path, producer, missing):
    """A valid release lacking image-owned helpers fails before producing a plausible ZIP."""
    root = paired(tmp_path, producer, omit=missing)
    archive = tmp_path / "missing kit.zip"
    with pytest.raises(ValueError, match="lacks the paired qualification helper"):
        producer.package(root, archive)
    assert not archive.exists()


def test_existing_package_is_preserved(tmp_path, producer):
    """A later export cannot replace earlier reviewed transport bytes."""
    root = paired(tmp_path, producer)
    archive = tmp_path / "earlier qualified baseline.zip"
    archive.write_bytes(b"preserve literal earlier artifact")
    with pytest.raises(FileExistsError):
        producer.package(root, archive)
    assert archive.read_bytes() == b"preserve literal earlier artifact"


def test_modified_operator_cannot_enter_transport_package(tmp_path, producer):
    """Reject drift against both full inventory and independent synthetic image rootfs."""
    root = paired(tmp_path, producer)
    (root / "operator/live_qualification/common.py").write_bytes(b"unpaired replacement")
    archive = tmp_path / "invalid.zip"
    with pytest.raises(ValueError):
        producer.package(root, archive)
    assert not archive.exists()


def test_selected_release_is_verified_before_any_child(tmp_path, producer, monkeypatch):
    """Actual packaged helpers reject identity drift and mutable launch tags without effects."""
    root = paired(tmp_path, producer)
    manifest_bytes = (root / "release.json").read_bytes()
    manifest = json.loads(manifest_bytes)
    common = load(root / "operator/live_qualification/common.py", "paired_common_test")
    selected = {
        "release_directory": str(root),
        "expected_release_manifest_sha256": _hash(manifest_bytes),
        "configuration_schema_version": 1,
        "expected_bridge_source_sha256": manifest["bridge"]["source_sha256"],
        "expected_bridge_protocol_version": 47,
        "expected_target_snapshot_schema_version": 35,
        "image_local_immutable_id": manifest["image"]["local_immutable_id"],
    }

    def unexpected_child(*args, **kwargs):
        pytest.fail("Qualification must verify the paired release before starting a child")

    monkeypatch.setattr(common.subprocess, "Popen", unexpected_child)
    assert (
        common.verify_selected_release(selected)["source_revision"]
        == manifest["source"]["revision"]
    )
    # Different Docker stores may expose the verified manifest or config ID.
    # These are authenticated by the same complete OCI graph, never by a tag.
    for name in ("oci_index_digest", "oci_manifest_digest", "oci_config_digest"):
        assert (
            common.verify_selected_release(
                {**selected, "image_local_immutable_id": manifest["image"][name]}
            )["image"]
            == manifest["image"]
        )
    for name, value in (
        ("configuration_schema_version", True),
        ("expected_bridge_source_sha256", "0" * 64),
        ("expected_bridge_protocol_version", 46),
        ("image_local_immutable_id", "sha256:" + "0" * 64),
    ):
        with pytest.raises(ValueError, match="configuration differs"):
            common.verify_selected_release({**selected, name: value})
    selected.update(
        host_mount_directory="/caller/new scratch data",
        server_mount_prefix="/data",
        mcp_command=[
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--mount",
            "type=bind,source=/caller/production.json,target=/run/sunny/configuration.json,readonly",
            "--mount",
            "type=bind,source=/caller/new scratch data,target=/data",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            "mutable-tag",
        ],
    )
    with pytest.raises(ValueError, match="exact immutable image"):
        common.Mcp(selected, tmp_path / "observations.jsonl")
    assert not list(root.rglob("__pycache__"))
