"""Verify release contracts with synthetic OCI bytes, without publishing or building.

These fixtures exercise the verifier and finite Docker orchestration. They do not
claim the synthetic image is executable, an actual Ubuntu build, or Live-qualified.
"""

from __future__ import annotations

import gzip
import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import tarfile
from pathlib import Path
from types import ModuleType

import pytest

PROJECT = Path(__file__).resolve().parents[2]


@pytest.fixture
def release_tool() -> ModuleType:
    """Import the standalone producer without invoking Docker or its CLI."""
    spec = importlib.util.spec_from_file_location(
        "sunny_release_test", PROJECT / "tools/release.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n")


def _tar(files: dict[str, bytes]) -> bytes:
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w") as archive:
        for name, data in files.items():
            entry = tarfile.TarInfo(name)
            entry.size = len(data)
            entry.mode = 0o644
            archive.addfile(entry, io.BytesIO(data))
    return output.getvalue()


def _bridge(root: Path, module_bytes: bytes = b"VALUE = 23\n") -> dict:
    root.mkdir(parents=True)
    (root / "__init__.py").write_bytes(module_bytes)
    _json(
        root / "bridge_contract.json",
        {"bridge_protocol_version": 46, "target_snapshot_schema_version": 35},
    )
    identity = _hash(
        b"sunny-remote-script-v1\n__init__.py\n" + _hash(module_bytes).encode() + b"\n"
    )
    (root / "source.sha256").write_text(identity + "\n")
    return {
        "source_sha256": identity,
        "contract": json.loads((root / "bridge_contract.json").read_text()),
        "files": {
            p.name: {"sha256": _hash(p.read_bytes()), "bytes": p.stat().st_size}
            for p in sorted(root.iterdir())
        },
    }


def _make_release(
    root: Path,
    tool: ModuleType,
    *,
    wrong_label: bool = False,
    wrong_rootfs: bool = False,
    before_payload: dict[str, bytes] | None = None,
    after_payload: dict[str, bytes] | None = None,
    upper_layer: dict[str, bytes] | None = None,
) -> dict:
    root.mkdir()
    bridge = _bridge(root / "native/Sunny")
    provenance_dir = root / "provenance"
    provenance_dir.mkdir()
    installer = root / "installer/windows"
    installer.mkdir(parents=True)
    (installer / "Sunny.ps1").write_bytes(b"# synthetic offline installer\n")
    operator = root / "operator"
    operator.mkdir()
    (operator / "doctor.py").write_bytes(b"# synthetic same-image doctor CLI\n")
    lock = json.loads((PROJECT / "release/build-inputs.json").read_text())
    base_tar = _tar({"etc/os-release": b"synthetic fixture\n"})
    base_layer = gzip.compress(base_tar, mtime=0)
    lock["base_image"]["layer_digests"] = ["sha256:" + _hash(base_layer)]
    _json(provenance_dir / "build-inputs.json", lock)
    packages = {}
    for stage in ("builder", "runtime"):
        inventory = "".join(
            name + "\t" + lock["ubuntu"]["packages"][name]["version"] + "\tamd64\n"
            for name in lock["ubuntu"][stage + "_packages"]
        )
        (provenance_dir / (stage + "-packages.tsv")).write_text(inventory)
        metadata = []
        for name in lock["ubuntu"][stage + "_packages"]:
            item = lock["ubuntu"]["packages"][name]
            metadata.append(
                "Package: "
                + name
                + "\nVersion: "
                + item["version"]
                + "\nFilename: "
                + item["filename"]
                + "\nSHA256: "
                + item["sha256"]
            )
        (provenance_dir / (stage + "-apt-metadata.txt")).write_text("\n\n".join(metadata) + "\n")
        packages[stage] = {
            line.split("\t")[0]: {"version": line.split("\t")[1], "architecture": "amd64"}
            for line in inventory.splitlines()
        }
    binary = b"synthetic executable fixture\n"
    source = {"revision": "4" * 40, "source_date_epoch": 1700000000}
    provenance = {
        "build_provenance_schema_version": 1,
        "source_revision": source["revision"],
        "source_date_epoch": source["source_date_epoch"],
        "product_version": "0.4.0",
        "configuration_schema_version": None,
        "configuration_contract": "legacy_environment",
        "build_inputs_sha256": _hash((provenance_dir / "build-inputs.json").read_bytes()),
        "dependencies": {
            name: {
                **lock["dependencies"][name],
                "actual_revision": lock["dependencies"][name]["revision"],
            }
            for name in ("json", "pugixml")
        },
        "builder_packages": packages["builder"],
        "tools": {"g++": "synthetic"},
        "binary_sha256": _hash(binary),
        "bridge": bridge,
        "native_host_qualification": "pending",
    }
    _json(provenance_dir / "build.json", provenance)
    payload = {"usr/local/bin/sunny-mcp": binary}
    for directory, prefix in (
        (root / "native/Sunny", "opt/sunny/remote-script/Sunny/"),
        (provenance_dir, "opt/sunny/release/"),
        (root / "installer", "opt/sunny/installer/"),
        (operator, "opt/sunny/operator/"),
    ):
        for path in directory.rglob("*"):
            if path.is_file():
                payload[prefix + path.relative_to(directory).as_posix()] = path.read_bytes()
    layer_tar = _tar({**(before_payload or {}), **payload, **(after_payload or {})})
    layer = gzip.compress(layer_tar, mtime=0)
    layers = [(base_tar, base_layer), (layer_tar, layer)]
    if upper_layer is not None:
        upper_tar = _tar(upper_layer)
        layers.append((upper_tar, gzip.compress(upper_tar, mtime=0)))
    config = json.dumps(
        {
            "architecture": "amd64",
            "os": "linux",
            "config": {
                "User": "sunny",
                "Entrypoint": ["sunny-mcp"],
                "Labels": {
                    "org.opencontainers.image.version": "0.4.0",
                    "org.opencontainers.image.revision": "5" * 40
                    if wrong_label
                    else source["revision"],
                    "org.sunny.build-inputs.sha256": provenance["build_inputs_sha256"],
                    "org.opencontainers.image.base.digest": lock["base_image"]["manifest_digest"],
                    "org.sunny.configuration.contract": "legacy_environment",
                },
            },
            "rootfs": {
                "type": "layers",
                "diff_ids": [
                    "sha256:" + ("0" * 64 if wrong_rootfs and index == 1 else _hash(data))
                    for index, (data, _) in enumerate(layers)
                ],
            },
        }
    ).encode()
    blobs = {}

    def descriptor(data: bytes, media: str) -> dict:
        digest = "sha256:" + _hash(data)
        blobs["blobs/sha256/" + digest[7:]] = data
        return {"mediaType": media, "digest": digest, "size": len(data)}

    config_desc = descriptor(config, "application/vnd.oci.image.config.v1+json")
    manifest_bytes = json.dumps(
        {
            "schemaVersion": 2,
            "mediaType": tool.OCI_MANIFEST,
            "config": config_desc,
            "layers": [
                descriptor(compressed, "application/vnd.oci.image.layer.v1.tar+gzip")
                for _, compressed in layers
            ],
        }
    ).encode()
    manifest_desc = descriptor(manifest_bytes, tool.OCI_MANIFEST)
    manifest_desc["platform"] = {"architecture": "amd64", "os": "linux"}
    index_bytes = json.dumps(
        {"schemaVersion": 2, "mediaType": tool.OCI_INDEX, "manifests": [manifest_desc]}
    ).encode()
    index_desc = descriptor(index_bytes, tool.OCI_INDEX)
    archive_files = {
        "oci-layout": b'{"imageLayoutVersion":"1.0.0"}',
        "index.json": json.dumps({"schemaVersion": 2, "manifests": [index_desc]}).encode(),
        **blobs,
    }
    (root / "image.tar").write_bytes(_tar(archive_files))
    manifest = {
        "release_manifest_schema_version": 1,
        "product": {"name": "Sunny", "version": "0.4.0"},
        "source": source,
        "configuration_schema_version": None,
        "configuration_contract": "legacy_environment",
        "bridge": bridge,
        "build_inputs_sha256": provenance["build_inputs_sha256"],
        "image": {
            "platform": "linux/amd64",
            "local_immutable_id": index_desc["digest"],
            "oci_index_digest": index_desc["digest"],
            "oci_manifest_digest": manifest_desc["digest"],
            "oci_config_digest": config_desc["digest"],
            "registry_reference": None,
            "registry_manifest_digest": None,
            "archive": "image.tar",
            "binary_sha256": _hash(binary),
        },
        "qualification": {
            "native_live": "pending",
            "production_configuration": "pending",
            "registry_publication": "not_authorized",
        },
        "files": tool.file_inventory(root),
    }
    _json(root / "release.json", manifest)
    return manifest


def _refresh_inventory(root: Path, tool: ModuleType) -> None:
    manifest = json.loads((root / "release.json").read_text())
    manifest["files"] = tool.file_inventory(root)
    _json(root / "release.json", manifest)


def test_verified_archive_distinguishes_index_manifest_config_and_registry(release_tool, tmp_path):
    """Use literal descriptor bytes as identity and leave publication/qualification pending."""
    root = tmp_path / "release"
    expected = _make_release(root, release_tool)
    trusted = _hash((root / "release.json").read_bytes())
    assert release_tool.verify_release(root, trusted) == expected
    identities = [
        expected["image"][key]
        for key in ("oci_index_digest", "oci_manifest_digest", "oci_config_digest")
    ]
    assert len(set(identities)) == 3
    assert expected["image"]["local_immutable_id"] == identities[0]
    assert expected["image"]["registry_manifest_digest"] is None
    assert expected["configuration_schema_version"] is None
    assert expected["qualification"]["native_live"] == "pending"
    with pytest.raises(ValueError, match="trusted checksum"):
        release_tool.verify_release(root, "f" * 64)


def test_wrong_native_pair_fails_even_with_self_consistent_manifest(release_tool, tmp_path):
    """Changing both external checksums cannot change the bridge inside the immutable image."""
    root = tmp_path / "release"
    _make_release(root, release_tool)
    shutil.rmtree(root / "native/Sunny")
    bridge = _bridge(root / "native/Sunny", b"VALUE = 99\n")
    provenance = json.loads((root / "provenance/build.json").read_text())
    provenance["bridge"] = bridge
    _json(root / "provenance/build.json", provenance)
    manifest = json.loads((root / "release.json").read_text())
    manifest["bridge"] = bridge
    _json(root / "release.json", manifest)
    _refresh_inventory(root, release_tool)
    with pytest.raises(ValueError, match="exact OCI image"):
        release_tool.verify_release(root)


def test_corrupt_oci_blob_is_detected_independently_of_payload_inventory(release_tool, tmp_path):
    """Rehashing image.tar in the release inventory cannot validate a changed OCI blob."""
    root = tmp_path / "release"
    manifest = _make_release(root, release_tool)
    with tarfile.open(root / "image.tar") as archive:
        files = {entry.name: archive.extractfile(entry).read() for entry in archive.getmembers()}
    blob = "blobs/sha256/" + manifest["image"]["oci_config_digest"][7:]
    files[blob] = files[blob].replace(b"amd64", b"arm64")
    (root / "image.tar").write_bytes(_tar(files))
    _refresh_inventory(root, release_tool)
    with pytest.raises(ValueError, match="OCI blob mismatch"):
        release_tool.verify_release(root)


def test_labels_must_agree_with_actual_build_provenance(release_tool, tmp_path):
    """Self-consistent OCI descriptors do not excuse a source-revision mismatch."""
    root = tmp_path / "release"
    _make_release(root, release_tool, wrong_label=True)
    with pytest.raises(ValueError, match="OCI labels"):
        release_tool.verify_release(root)


@pytest.mark.parametrize("change", ["registry", "qualified", "local_id", "source", "schema"])
def test_manifest_cannot_invent_release_claims(release_tool, tmp_path, change):
    """Reject published, qualified, identity, source and schema claims absent from the image."""
    root = tmp_path / "release"
    manifest = _make_release(root, release_tool)
    if change == "registry":
        manifest["image"]["registry_manifest_digest"] = manifest["image"]["local_immutable_id"]
    elif change == "qualified":
        manifest["qualification"]["native_live"] = "passed"
    elif change == "local_id":
        manifest["image"]["local_immutable_id"] = "sha256:" + "f" * 64
    elif change == "source":
        manifest["source"]["revision"] = "f" * 40
    else:
        manifest["configuration_schema_version"] = 1
    _json(root / "release.json", manifest)
    with pytest.raises(ValueError):
        release_tool.verify_release(root)


def test_unsafe_archive_is_rejected_without_extracting_files(release_tool, tmp_path):
    """Untrusted archive member names cannot escape into a local directory."""
    archive = tmp_path / "image.tar"
    archive.write_bytes(_tar({"../../escape": b"unsafe"}))
    with pytest.raises(ValueError, match="Unsafe image archive path"):
        release_tool.archive_info(archive)
    assert not (tmp_path.parent / "escape").exists()


@pytest.mark.parametrize("opaque", [False, True], ids=["explicit", "opaque"])
@pytest.mark.parametrize("marker_first", [False, True], ids=["marker-last", "marker-first"])
def test_managed_whiteouts_rejected_in_both_tar_orders(
    release_tool, tmp_path, opaque, marker_first
):
    """Same-layer additions survive OCI whiteouts; the producer refuses managed deletions."""
    prefix = "opt/sunny/remote-script/Sunny/"
    extra = (prefix + "unreported_extra.py", b"VALUE = 99\n")
    marker = (prefix + (".wh..wh..opq" if opaque else ".wh.unreported_extra.py"), b"")
    entries = dict([marker, extra] if marker_first else [extra, marker])
    root = tmp_path / "release"
    # Opaque markers precede the declared bridge files so the old streaming
    # parser incorrectly erased the extra while retaining the expected payload.
    _make_release(
        root,
        release_tool,
        before_payload=entries if opaque else None,
        after_payload=entries if not opaque else None,
    )
    trusted = _hash((root / "release.json").read_bytes())
    with pytest.raises(ValueError, match="Managed image whiteouts are unsupported"):
        release_tool.verify_release(root, trusted)


@pytest.mark.parametrize(
    "marker",
    [
        ".wh.opt",
        ".wh..wh..opq",
        "opt/.wh.sunny",
        "opt/sunny/.wh.remote-script",
        "opt/sunny/.wh..wh..opq",
        "usr/local/bin/.wh.sunny-mcp",
        "opt/sunny/release/.wh.build.json",
        "opt/sunny/installer/.wh.windows",
        "opt/sunny/operator/.wh.doctor.py",
    ],
)
def test_whiteouts_of_managed_paths_and_ancestors_rejected(release_tool, tmp_path, marker):
    """Deletion or opacity of every managed root and its parents is unsupported."""
    root = tmp_path / "release"
    _make_release(root, release_tool, upper_layer={marker: b""})
    trusted = _hash((root / "release.json").read_bytes())
    with pytest.raises(ValueError, match="Managed image whiteouts are unsupported"):
        release_tool.verify_release(root, trusted)


@pytest.mark.parametrize("leaf", [".wh.", ".wh..", ".wh..."])
def test_whiteout_targets_cannot_name_parent_or_current_directory(release_tool, tmp_path, leaf):
    """A whiteout target must be a file basename, so dot aliases cannot bypass root checks."""
    root = tmp_path / "release"
    _make_release(root, release_tool, upper_layer={"opt/sunny/unused/" + leaf: b""})
    trusted = _hash((root / "release.json").read_bytes())
    with pytest.raises(ValueError, match="Invalid image whiteout name"):
        release_tool.verify_release(root, trusted)


@pytest.mark.parametrize(
    "alias",
    [
        "usr/local/bin/./sunny-mcp",
        "opt//sunny/remote-script/Sunny/__init__.py",
        "opt/./sunny/remote-script/Sunny/__init__.py",
        "opt/sunny/remote-script/Sunny/./.wh.__init__.py",
    ],
)
def test_layer_path_aliases_cannot_bypass_managed_payload_checks(release_tool, tmp_path, alias):
    """A valid upper layer cannot disguise a protected overwrite with normalized tar paths."""
    root = tmp_path / "release"
    _make_release(root, release_tool, upper_layer={alias: b"VALUE = 99\n"})
    trusted = _hash((root / "release.json").read_bytes())
    with pytest.raises(ValueError, match="Noncanonical image layer path"):
        release_tool.verify_release(root, trusted)


@pytest.mark.parametrize(
    "alias",
    [".//opt/sunny/remote-script/Sunny/__init__.py", ".//usr/local/bin/sunny-mcp"],
)
def test_leading_dot_prefix_cannot_disguise_absolute_layer_paths(release_tool, tmp_path, alias):
    """Relative admission is checked after the permitted prefix and before directory trimming."""
    root = tmp_path / "release"
    _make_release(root, release_tool, upper_layer={alias: b"VALUE = 99\n"})
    trusted = _hash((root / "release.json").read_bytes())
    with pytest.raises(ValueError, match="Unsafe image layer path"):
        release_tool.verify_release(root, trusted)


def test_unrelated_whiteouts_and_leading_dot_slash_are_admitted(release_tool, tmp_path):
    """Ordinary package cleanup outside the managed payload does not narrow producer support."""
    root = tmp_path / "release"
    expected = _make_release(
        root,
        release_tool,
        upper_layer={
            "./var/cache/apt/.wh.obsolete": b"",
            "./var/lib/apt/lists/.wh..wh..opq": b"",
        },
    )
    trusted = _hash((root / "release.json").read_bytes())
    assert release_tool.verify_release(root, trusted) == expected


def test_load_verifies_before_docker_and_accepts_destination_config_id(
    release_tool, tmp_path, monkeypatch
):
    """A classic store's config ID is distinct from the original store's index ID."""
    root = tmp_path / "release"
    manifest = _make_release(root, release_tool)
    calls = []

    def docker(*arguments, **kwargs):
        calls.append(arguments)
        if arguments[1:3] == ("image", "load"):
            return "Loaded image"
        if arguments[-1] != manifest["image"]["oci_config_digest"]:
            raise subprocess.CalledProcessError(1, arguments)
        return json.dumps(
            [
                {
                    "Id": manifest["image"]["oci_config_digest"],
                    "Os": "linux",
                    "Architecture": "amd64",
                    "Config": {
                        "Labels": {
                            "org.opencontainers.image.revision": manifest["source"]["revision"]
                        }
                    },
                }
            ]
        )

    monkeypatch.setattr(release_tool, "run", docker)
    loaded = release_tool.load_release(root, None)
    assert loaded["loaded_image_local_immutable_id"] == manifest["image"]["oci_config_digest"]
    assert loaded["oci_manifest_digest"] == manifest["image"]["oci_manifest_digest"]
    calls.clear()
    (root / "installer/windows/Sunny.ps1").write_bytes(b"wrong installer")
    with pytest.raises(ValueError, match="inventory mismatch"):
        release_tool.load_release(root, None)
    assert calls == []


def test_export_uses_immutable_id_and_stopped_container(release_tool, tmp_path, monkeypatch):
    """The producer copies image payloads without starting a server or claiming RepoDigests."""
    fixture = tmp_path / "fixture"
    expected = _make_release(fixture, release_tool)
    identity = expected["image"]["local_immutable_id"]
    calls = []

    def docker(*arguments, **kwargs):
        calls.append(arguments)
        operation = arguments[1:3]
        if operation == ("image", "inspect"):
            return json.dumps(
                [
                    {
                        "Id": identity,
                        "Os": "linux",
                        "Architecture": "amd64",
                        "RepoDigests": ["local@" + identity],
                    }
                ]
            )
        if arguments[1] == "create":
            assert arguments == ("docker", "create", "--network", "none", identity)
            return "stopped-container"
        if arguments[1] == "cp":
            source = {
                "/opt/sunny/remote-script/Sunny": fixture / "native/Sunny",
                "/opt/sunny/release": fixture / "provenance",
                "/opt/sunny/installer": fixture / "installer",
                "/opt/sunny/operator": fixture / "operator",
            }[arguments[2].split(":", 1)[1]]
            shutil.copytree(source, Path(arguments[3]))
        if operation == ("image", "save"):
            assert arguments[-1] == identity
            shutil.copyfile(fixture / "image.tar", Path(arguments[-2]))
        return ""

    monkeypatch.setattr(release_tool, "run", docker)
    output = tmp_path / "export"
    manifest = release_tool.export_image(identity, output)
    assert release_tool.verify_release(output) == manifest
    assert (output / "operator/doctor.py").read_bytes() == (
        fixture / "operator/doctor.py"
    ).read_bytes()
    assert next(item for item in manifest["files"] if item["path"] == "operator/doctor.py") == {
        "path": "operator/doctor.py",
        "bytes": (fixture / "operator/doctor.py").stat().st_size,
        "sha256": _hash((fixture / "operator/doctor.py").read_bytes()),
    }
    assert manifest["image"]["registry_reference"] is None
    assert ("docker", "rm", "stopped-container") in calls
    assert not any(arguments[1] in ("run", "start", "push", "pull") for arguments in calls)
    with pytest.raises(ValueError, match="Output exists"):
        release_tool.export_image(identity, output)


def test_dirty_source_refuses_build_before_docker(release_tool, tmp_path, monkeypatch):
    """Release builds do not attribute uncommitted source to an existing revision."""
    calls = []

    def command(*arguments, **kwargs):
        calls.append(arguments)
        return " M Dockerfile"

    monkeypatch.setattr(release_tool, "run", command)
    with pytest.raises(ValueError, match="clean committed source"):
        release_tool.build_release(tmp_path / "out", "sunny:fixture")
    assert len(calls) == 1 and calls[0][0] == "git"


def test_locked_docker_defaults_match_machine_inputs(release_tool):
    """Direct Docker defaults and the release producer resolve the same pinned roots."""
    lock = json.loads((PROJECT / "release/build-inputs.json").read_text())
    dockerfile = (PROJECT / "Dockerfile").read_text()
    assert dockerfile.count("FROM " + lock["base_image"]["reference"]) == 2
    assert dockerfile.count("ARG SUNNY_APT_SNAPSHOT=" + lock["ubuntu"]["snapshot"]) == 2
    for stage, argument in (
        ("builder", "SUNNY_BUILD_PACKAGES"),
        ("runtime", "SUNNY_RUNTIME_PACKAGES"),
    ):
        packages = " ".join(
            name + "=" + lock["ubuntu"]["packages"][name]["version"]
            for name in lock["ubuntu"][stage + "_packages"]
        )
        assert "ARG " + argument + '="' + packages + '"' in dockerfile
    assert "--uid 1001 --gid 1001" in dockerfile
    assert "-DSUNNY_RELEASE_BUILD=ON" in dockerfile
    assert "COPY --from=builder /build/tools/windows/ /opt/sunny/installer/windows/" in dockerfile
    assert "COPY tools/doctor.py ./tools/doctor.py" in dockerfile
    assert "COPY --from=builder /build/tools/doctor.py /opt/sunny/operator/doctor.py" in dockerfile


def test_build_freezes_actual_committed_tree_before_concurrent_edits(
    release_tool, tmp_path, monkeypatch
):
    """A real git archive fixes source bytes even if worktree editing resumes during the build."""
    repository = tmp_path / "source"
    repository.mkdir()
    (repository / "release").mkdir()
    shutil.copyfile(PROJECT / "release/build-inputs.json", repository / "release/build-inputs.json")
    (repository / "pyproject.toml").write_text('[project]\nversion = "0.4.0"\n')
    (repository / "marker.txt").write_text("committed bytes")
    for arguments in (
        ("init", "-q"),
        ("add", "."),
        (
            "-c",
            "user.name=Release fixture",
            "-c",
            "user.email=fixture@invalid",
            "commit",
            "-qm",
            "fixture",
        ),
    ):
        subprocess.run(["git", *arguments], cwd=repository, check=True, capture_output=True)
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=repository, text=True
    ).strip()
    image_id, config_id = "sha256:" + "1" * 64, "sha256:" + "2" * 64
    actual_run = release_tool.run
    saw_build = []

    def command(*arguments, **kwargs):
        if arguments[0] == "git":
            return actual_run(*arguments, **kwargs)
        if arguments[1:3] == ("buildx", "build"):
            (repository / "marker.txt").write_text("concurrent uncommitted bytes")
            context = Path(arguments[-1])
            assert context != repository
            assert (context / "marker.txt").read_text() == "committed bytes"
            assert "SUNNY_SOURCE_REVISION=" + revision in arguments
            metadata = Path(arguments[arguments.index("--metadata-file") + 1])
            _json(
                metadata,
                {"containerimage.digest": image_id, "containerimage.config.digest": config_id},
            )
            saw_build.append(context)
            return ""
        assert arguments == ("docker", "image", "inspect", "sunny:fixture")
        return json.dumps([{"Id": image_id}])

    monkeypatch.setattr(release_tool, "ROOT", repository)
    monkeypatch.setattr(release_tool, "LOCK", repository / "release/build-inputs.json")
    monkeypatch.setattr(release_tool, "run", command)
    monkeypatch.setattr(
        release_tool, "export_image", lambda identity, output, metadata: {"identity": identity}
    )
    assert release_tool.build_release(tmp_path / "out", "sunny:fixture") == {"identity": image_id}
    assert len(saw_build) == 1


@pytest.mark.parametrize("change", ["sha256", "filename", "version", "missing"])
def test_apt_inputs_are_required_not_just_documentary(release_tool, tmp_path, change):
    """The lock's package archive SHA256/version/path must match actual apt index records."""
    root = tmp_path / "release"
    _make_release(root, release_tool)
    lock = json.loads((root / "provenance/build-inputs.json").read_text())
    path = root / "provenance/runtime-apt-metadata.txt"
    value = path.read_text()
    expected = lock["ubuntu"]["packages"]["libstdc++6"]
    if change == "missing":
        value = ""
    else:
        value = value.replace(expected[change], "wrong input")
    path.write_text(value)
    with pytest.raises(ValueError, match="Apt"):
        release_tool.apt_metadata(path, lock, "runtime")


@pytest.mark.parametrize("case", ["valid", "dirty", "wrong_revision", "developer"])
def test_release_cmake_admits_only_clean_locked_dependencies(tmp_path, case):
    """Configure small real Git dependencies, without building or changing Sunny's caches."""
    cmake = shutil.which("cmake")
    if not cmake:
        pytest.skip("CMake is required for the standalone dependency-admission fixture")
    project = tmp_path / "project"
    (project / "release").mkdir(parents=True)
    lock = json.loads((PROJECT / "release/build-inputs.json").read_text())
    sources = {}
    for name in ("json", "pugixml"):
        source = tmp_path / name
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.28)\nadd_library(" + name + "_fixture INTERFACE)\n"
        )
        for arguments in (
            ("init", "-q"),
            ("add", "."),
            (
                "-c",
                "user.name=Lock fixture",
                "-c",
                "user.email=fixture@invalid",
                "commit",
                "-qm",
                "fixture",
            ),
        ):
            subprocess.run(["git", *arguments], cwd=source, capture_output=True, check=True)
        revision = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=source, text=True
        ).strip()
        lock["dependencies"][name]["revision"] = "f" * 40 if case == "wrong_revision" else revision
        sources[name] = source
    if case in ("dirty", "developer"):
        with (sources["json"] / "CMakeLists.txt").open("a") as stream:
            stream.write("# uncommitted dependency bytes\n")
    _json(project / "release/build-inputs.json", lock)
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\nproject(LockFixture LANGUAGES CXX)\n"
        "include(FetchContent)\nset(SUNNY_BUILD_TESTS OFF)\nset(SUNNY_BUILD_PYTHON_BINDINGS OFF)\n"
        f'include("{(PROJECT / "cmake/Dependencies.cmake").as_posix()}")\n'
    )
    result = subprocess.run(
        [
            cmake,
            "-S",
            str(project),
            "-B",
            str(tmp_path / "build"),
            "-DSUNNY_RELEASE_BUILD=" + ("OFF" if case == "developer" else "ON"),
            "-DCMAKE_DISABLE_FIND_PACKAGE_nlohmann_json=TRUE",
            "-DCMAKE_DISABLE_FIND_PACKAGE_pugixml=TRUE",
            "-DFETCHCONTENT_SOURCE_DIR_JSON=" + str(sources["json"]),
            "-DFETCHCONTENT_SOURCE_DIR_PUGIXML=" + str(sources["pugixml"]),
        ],
        capture_output=True,
        text=True,
        timeout=60,
    )
    if case in ("valid", "developer"):
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0
        assert "does not match clean locked revision" in result.stderr


def test_oci_config_rootfs_matches_uncompressed_layer_bytes(release_tool, tmp_path):
    """Independently valid OCI blob hashes cannot excuse an invalid runtime rootfs digest."""
    root = tmp_path / "release"
    _make_release(root, release_tool, wrong_rootfs=True)
    with pytest.raises(ValueError, match="rootfs digest mismatch"):
        release_tool.verify_release(root)


def test_builder_records_actual_dependencies_tools_and_binary(release_tool, tmp_path, monkeypatch):
    """Record provenance from real tiny Git checkouts and literal generated payload bytes."""
    fixture = tmp_path / "fixture"
    _make_release(fixture, release_tool)
    project = tmp_path / "builder"
    build = project / ".bin"
    build.mkdir(parents=True)
    (project / "pyproject.toml").write_text('[project]\nversion = "0.4.0"\n')
    lock = json.loads((fixture / "provenance/build-inputs.json").read_text())
    for name in ("json", "pugixml"):
        source = build / "_deps" / (name + "-src")
        source.mkdir(parents=True)
        (source / "source.txt").write_bytes(b"literal fixture dependency")
        for arguments in (
            ("init", "-q"),
            ("add", "."),
            (
                "-c",
                "user.name=Builder fixture",
                "-c",
                "user.email=fixture@invalid",
                "commit",
                "-qm",
                "fixture",
            ),
        ):
            subprocess.run(["git", *arguments], cwd=source, capture_output=True, check=True)
        lock["dependencies"][name]["revision"] = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=source, text=True
        ).strip()
    (project / "release").mkdir()
    _json(project / "release/build-inputs.json", lock)
    shutil.copytree(fixture / "native/Sunny", build / "remote_script/Sunny")
    (build / "sunny-mcp").write_bytes(b"literal fixture binary")
    actual_run = release_tool.run

    def command(*arguments, **kwargs):
        if arguments[0] == "git" and "rev-parse" in arguments:
            return actual_run(*arguments, **kwargs)
        assert arguments[-1] == "--version"
        return arguments[0] + " observed fixture version"

    monkeypatch.setattr(release_tool, "ROOT", project)
    monkeypatch.setattr(release_tool, "LOCK", project / "release/build-inputs.json")
    monkeypatch.setattr(release_tool, "run", command)
    from argparse import Namespace

    output = build / "release"
    release_tool.record_build(
        Namespace(
            source_revision="4" * 40,
            source_date_epoch=1700000000,
            build_directory=build,
            packages=fixture / "provenance/builder-packages.tsv",
            apt_metadata=fixture / "provenance/builder-apt-metadata.txt",
            output=output,
        )
    )
    provenance = json.loads((output / "build.json").read_text())
    assert provenance["binary_sha256"] == _hash(b"literal fixture binary")
    assert provenance["bridge"] == json.loads((fixture / "release.json").read_text())["bridge"]
    assert (
        provenance["dependencies"]["json"]["actual_revision"]
        == lock["dependencies"]["json"]["revision"]
    )
    assert set(provenance["tools"]) == {"g++", "cmake", "git", "ninja", "python3"}
    assert (output / "builder-apt-metadata.txt").read_bytes() == (
        fixture / "provenance/builder-apt-metadata.txt"
    ).read_bytes()
    assert provenance["configuration_schema_version"] is None
    assert provenance["native_host_qualification"] == "pending"


@pytest.mark.parametrize("change", ["missing", "modified"])
def test_doctor_requires_exact_same_image_bytes(release_tool, tmp_path, change):
    """An updated external inventory cannot admit missing or mismatched operator tooling."""
    root = tmp_path / "release"
    _make_release(root, release_tool)
    doctor = root / "operator/doctor.py"
    if change == "missing":
        doctor.unlink()
    else:
        doctor.write_bytes(b"# CLI from another image\n")
    _refresh_inventory(root, release_tool)
    with pytest.raises(ValueError, match="Missing operator doctor CLI|exact OCI image"):
        release_tool.verify_release(root)
