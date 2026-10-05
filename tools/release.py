"""Produce and verify a paired OCI image/native bridge without publication.

Commands build/export create a fresh directory containing image.tar, native/Sunny,
installer, operator/doctor.py, provenance and release.json. verify needs no Docker
or network. load verifies that same archive before importing it; it never rebuilds
or edits native payloads.
The primary producer uses Docker's OCI-capable image store. A destination store
may expose a different local ID; load reports that identity separately.
A local ID, archived OCI digest and published registry digest are separate fields.
No command here publishes an image or claims native Live qualification.
"""

from __future__ import annotations

import argparse
import ast
import gzip
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / "release" / "build-inputs.json"
DIGEST = re.compile(r"sha256:[0-9a-f]{64}\Z")
REVISION = re.compile(r"[0-9a-f]{40}\Z")
OCI_INDEX = "application/vnd.oci.image.index.v1+json"
DOCKER_INDEX = "application/vnd.docker.distribution.manifest.list.v2+json"
OCI_MANIFEST = "application/vnd.oci.image.manifest.v1+json"
DOCKER_MANIFEST = "application/vnd.docker.distribution.manifest.v2+json"
BRIDGE_PREFIX = "opt/sunny/remote-script/Sunny/"
PROVENANCE_PREFIX = "opt/sunny/release/"
INSTALLER_PREFIX = "opt/sunny/installer/"
OPERATOR_PREFIX = "opt/sunny/operator/"
BINARY = "usr/local/bin/sunny-mcp"


def sha256(path: Path) -> str:
    """Hash a file without loading an image archive into memory."""
    with path.open("rb") as stream:
        return stream_digest(stream)


def stream_digest(stream) -> str:
    """Hash a bounded-memory binary stream."""
    result = hashlib.sha256()
    while block := stream.read(1024 * 1024):
        result.update(block)
    return result.hexdigest()


def write_json(path: Path, value: dict) -> None:
    """Write readable machine metadata into an already reserved directory."""
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run(*arguments: str, cwd: Path | None = None, timeout: int = 60) -> str:
    """Run a finite command with literal arguments and no shell."""
    result = subprocess.run(
        arguments, cwd=cwd, capture_output=True, text=True, check=True, timeout=timeout
    )
    return result.stdout.strip()


def require(condition: bool, message: str) -> None:
    """Reject a contract violation before publishing or loading a result."""
    if not condition:
        raise ValueError(message)


def check_buildkit_metadata(metadata: dict, image: dict) -> None:
    """Correlate reported fields without inventing a config digest omitted by Buildx."""
    require(isinstance(metadata, dict), "BuildKit metadata requires an object")
    digest = metadata.get("containerimage.digest")
    require(
        isinstance(digest, str)
        and DIGEST.fullmatch(digest)
        and digest in (image["oci_index_digest"], image["oci_manifest_digest"]),
        "BuildKit OCI digest differs from archive",
    )
    if "containerimage.config.digest" in metadata:
        config = metadata["containerimage.config.digest"]
        require(
            isinstance(config, str)
            and DIGEST.fullmatch(config)
            and config == image["oci_config_digest"],
            "BuildKit config digest differs from archive",
        )


def bridge_info(root: Path) -> dict:
    """Independently verify the generated adapter's source stream and contract."""
    digest = hashlib.sha256(b"sunny-remote-script-v1\n")
    files = {}
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), "Native bridge contains a symbolic link")
        if not path.is_file():
            continue
        name = path.relative_to(root).as_posix()
        files[name] = {"sha256": sha256(path), "bytes": path.stat().st_size}
        if path.suffix == ".py":
            require(
                bool(re.fullmatch(r"(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.py", name)),
                "Unsupported bridge module name: " + name,
            )
            digest.update((name + "\n" + files[name]["sha256"] + "\n").encode())
        else:
            require(name in ("source.sha256", "bridge_contract.json"), "Unexpected bridge file")
    require(any(name.endswith(".py") for name in files), "Native bridge has no modules")
    identity = digest.hexdigest()
    require((root / "source.sha256").read_text().strip() == identity, "Bridge checksum mismatch")
    contract = json.loads((root / "bridge_contract.json").read_text())
    require(
        set(contract) == {"bridge_protocol_version", "target_snapshot_schema_version"}
        and all(type(value) is int and 0 < value <= 4294967295 for value in contract.values()),
        "Invalid bridge contract",
    )
    return {"source_sha256": identity, "contract": contract, "files": files}


def native_configuration_contract(root: Path) -> dict:
    """Read literal adapter constants without executing exported Python code."""
    path = root / "configuration.py"
    require(
        path.is_file() and path.stat().st_size <= 131072,
        "Missing bounded native configuration consumer",
    )
    authority = {}
    try:
        statements = ast.parse(path.read_bytes()).body
    except (SyntaxError, UnicodeError, ValueError) as error:
        raise ValueError("Invalid native configuration consumer source") from error
    for statement in statements:
        if not isinstance(statement, ast.Assign) or len(statement.targets) != 1:
            continue
        target = statement.targets[0]
        if isinstance(target, ast.Name) and target.id in (
            "CONFIGURATION_SCHEMA_VERSION",
            "CONFIGURATION_CONTRACT",
        ):
            require(target.id not in authority, "Duplicate native configuration authority")
            authority[target.id] = ast.literal_eval(statement.value)
    require(
        type(authority.get("CONFIGURATION_SCHEMA_VERSION")) is int
        and authority["CONFIGURATION_SCHEMA_VERSION"] == 1
        and authority.get("CONFIGURATION_CONTRACT") == "versioned_json",
        "Unsupported native configuration consumer contract",
    )
    return {
        "configuration_schema_version": authority["CONFIGURATION_SCHEMA_VERSION"],
        "configuration_contract": authority["CONFIGURATION_CONTRACT"],
    }


def configuration_identity(lock: dict) -> dict:
    """Admit the implemented production contract or honest archived legacy metadata."""
    identity = {
        key: lock[key] for key in ("configuration_schema_version", "configuration_contract")
    }
    require(
        (
            type(identity["configuration_schema_version"]) is int
            and identity["configuration_schema_version"] == 1
            and identity["configuration_contract"] == "versioned_json"
        )
        or (
            identity["configuration_schema_version"] is None
            and identity["configuration_contract"] == "legacy_environment"
        ),
        "Unsupported configuration contract",
    )
    return identity


def package_inventory(path: Path, lock: dict, stage: str) -> dict:
    """Check pinned apt roots and retain the entire installed package inventory."""
    packages = {}
    for line in path.read_text().splitlines():
        name, version, architecture = line.split("\t")
        require(name not in packages, "Duplicate installed package: " + name)
        packages[name] = {"version": version, "architecture": architecture}
    for name in lock["ubuntu"][stage + "_packages"]:
        require(
            name in packages
            and packages[name]["version"] == lock["ubuntu"]["packages"][name]["version"],
            "Installed " + stage + " package differs from lock: " + name,
        )
    return packages


def apt_metadata(path: Path, lock: dict, stage: str) -> None:
    """Check the signed apt index records used to select each exact root package."""
    found = set()
    roots = lock["ubuntu"][stage + "_packages"]
    for paragraph in path.read_text().split("\n\n"):
        record = {}
        for line in paragraph.splitlines():
            if line and not line[0].isspace() and ": " in line:
                key, value = line.split(": ", 1)
                record[key] = value
        name = record.get("Package")
        if name not in roots:
            continue
        if "Filename" not in record and "SHA256" not in record:
            # apt-cache can also display the installed dpkg status paragraph;
            # only a complete archive index record can satisfy the lock.
            continue
        expected = lock["ubuntu"]["packages"][name]
        require(
            record.get("Version") == expected["version"]
            and record.get("Filename") == expected["filename"]
            and record.get("SHA256") == expected["sha256"],
            "Apt index differs from locked package input: " + name,
        )
        found.add(name)
    require(found == set(roots), "Apt root package metadata is incomplete")


def record_build(args) -> None:
    """Capture actual compiler, dependency and binary provenance inside the builder."""
    lock = json.loads(LOCK.read_text())
    args.output.mkdir()
    shutil.copyfile(LOCK, args.output / "build-inputs.json")
    shutil.copyfile(args.packages, args.output / "builder-packages.tsv")
    apt_metadata(args.apt_metadata, lock, "builder")
    shutil.copyfile(args.apt_metadata, args.output / "builder-apt-metadata.txt")
    identity = configuration_identity(lock)
    require(
        identity["configuration_schema_version"] == 1,
        "New release requires the production configuration consumer",
    )
    compiled_identity = json.loads(
        run(str(args.build_directory / "sunny-mcp"), "--configuration-contract")
    )
    require(
        isinstance(compiled_identity, dict)
        and set(compiled_identity) == set(identity)
        and type(compiled_identity.get("configuration_schema_version")) is int
        and compiled_identity == identity,
        "Compiled configuration consumer differs from build input contract",
    )
    require(
        native_configuration_contract(args.build_directory / "remote_script" / "Sunny") == identity,
        "Native configuration consumer differs from compiled client",
    )
    dependencies = {}
    for name in ("json", "pugixml"):
        source = args.build_directory / "_deps" / (name.lower() + "-src")
        revision = run("git", "-C", str(source), "rev-parse", "HEAD")
        require(revision == lock["dependencies"][name]["revision"], "Dependency revision mismatch")
        dependencies[name] = {**lock["dependencies"][name], "actual_revision": revision}
    provenance = {
        "build_provenance_schema_version": 1,
        "source_revision": args.source_revision,
        "source_date_epoch": args.source_date_epoch,
        "product_version": json.loads(
            (ROOT / "pyproject.toml").read_text().split("version = ", 1)[1].splitlines()[0]
        ),
        "configuration_schema_version": lock["configuration_schema_version"],
        "configuration_contract": lock["configuration_contract"],
        "compiled_configuration_consumer": compiled_identity,
        "build_inputs_sha256": sha256(LOCK),
        "dependencies": dependencies,
        "builder_packages": package_inventory(args.packages, lock, "builder"),
        "tools": {
            name: run(name, "--version").splitlines()[0]
            for name in ("g++", "cmake", "git", "ninja", "python3")
        },
        "binary_sha256": sha256(args.build_directory / "sunny-mcp"),
        "bridge": bridge_info(args.build_directory / "remote_script" / "Sunny"),
        "native_host_qualification": "pending",
    }
    write_json(args.output / "build.json", provenance)


def archive_info(path: Path) -> dict:
    """Verify actual OCI descriptors, configs, layers and managed files in an archive.

    This reads the archive without extracting paths to disk. Registry publication
    is deliberately not inferred from Docker RepoDigests or archive annotations.
    The producer only adds its managed payload. Managed whiteouts and alternate
    layer path spellings are unsupported and rejected before payload comparison.
    """
    with tarfile.open(path, "r:*") as archive:
        members = {}
        for entry in archive.getmembers():
            name = entry.name.removeprefix("./")
            require(
                not name.startswith("/") and ".." not in PurePosixPath(name).parts,
                "Unsafe image archive path",
            )
            if entry.isdir():
                continue
            require(
                entry.isfile() and name not in members, "Invalid or duplicate image archive member"
            )
            members[name] = entry

        def read(name: str, limit: int = 8 * 1024 * 1024) -> bytes:
            require(
                name in members and members[name].size <= limit, "Missing/oversized OCI metadata"
            )
            return archive.extractfile(members[name]).read()

        require("oci-layout" in members, "Export requires Docker's OCI-capable image store")
        require(json.loads(read("oci-layout")) == {"imageLayoutVersion": "1.0.0"}, "Bad OCI layout")
        index = json.loads(read("index.json"))
        require(
            index.get("schemaVersion") == 2 and len(index["manifests"]) == 1, "Export one image"
        )
        checked = {}
        candidates = []

        def descriptor(item: dict) -> tuple[str, bytes | None]:
            identity = item["digest"]
            require(bool(DIGEST.fullmatch(identity)), "Unsupported OCI digest")
            name = "blobs/sha256/" + identity.split(":")[1]
            require(
                name in members and members[name].size == item["size"],
                "OCI descriptor size mismatch",
            )
            if identity not in checked:
                require(
                    stream_digest(archive.extractfile(members[name])) == identity[7:],
                    "OCI blob mismatch",
                )
                checked[identity] = name
            media = item["mediaType"]
            if media in (OCI_INDEX, DOCKER_INDEX, OCI_MANIFEST, DOCKER_MANIFEST):
                return name, read(name)
            return name, None

        def visit(item: dict) -> None:
            name, data = descriptor(item)
            require(data is not None, "Unexpected root OCI descriptor")
            node = json.loads(data)
            require(node.get("schemaVersion") == 2, "Bad OCI schema version")
            if item["mediaType"] in (OCI_INDEX, DOCKER_INDEX):
                for child in node["manifests"]:
                    visit(child)
            else:
                config_name, _ = descriptor(node["config"])
                config = json.loads(read(config_name))
                for layer in node["layers"]:
                    descriptor(layer)
                if config.get("os") == "linux" and config.get("architecture") == "amd64":
                    candidates.append((item, node, config))

        root = index["manifests"][0]
        visit(root)
        require(len(candidates) == 1, "Release must contain exactly one linux/amd64 image")
        manifest, node, config = candidates[0]
        payload = {}
        managed_roots = (
            BINARY,
            BRIDGE_PREFIX.rstrip("/"),
            PROVENANCE_PREFIX.rstrip("/"),
            INSTALLER_PREFIX.rstrip("/"),
            OPERATOR_PREFIX.rstrip("/"),
        )
        diff_ids = config.get("rootfs", {}).get("diff_ids", [])
        require(len(diff_ids) == len(node["layers"]), "OCI rootfs layer count mismatch")
        for position, layer in enumerate(node["layers"]):
            require(
                layer["mediaType"]
                in (
                    "application/vnd.oci.image.layer.v1.tar+gzip",
                    "application/vnd.oci.image.layer.v1.tar",
                    "application/vnd.docker.image.rootfs.diff.tar.gzip",
                ),
                "Unsupported release layer compression",
            )
            compressed = archive.extractfile(members[checked[layer["digest"]]])
            if layer["mediaType"].endswith("gzip"):
                with gzip.GzipFile(fileobj=compressed) as uncompressed:
                    actual_diff_id = stream_digest(uncompressed)
            else:
                actual_diff_id = stream_digest(compressed)
            require(diff_ids[position] == "sha256:" + actual_diff_id, "OCI rootfs digest mismatch")
            blob = archive.extractfile(members[checked[layer["digest"]]])
            with tarfile.open(fileobj=blob, mode="r|*") as contents:
                for entry in contents:
                    name = entry.name.removeprefix("./")
                    require(
                        not name.startswith("/") and ".." not in PurePosixPath(name).parts,
                        "Unsafe image layer path",
                    )
                    if entry.isdir():
                        name = name.rstrip("/")
                    require(
                        name == PurePosixPath(name).as_posix() or (not name and entry.isdir()),
                        "Noncanonical image layer path",
                    )
                    parent, _, leaf = name.rpartition("/")
                    if leaf.startswith(".wh."):
                        require(
                            leaf == ".wh..wh..opq" or leaf[4:] not in ("", ".", ".."),
                            "Invalid image whiteout name",
                        )
                        target = (parent + "/" if parent else "") + leaf[4:]
                        if leaf == ".wh..wh..opq":
                            target = parent
                        target = target.rstrip("/")
                        # OCI whiteouts only remove lower-layer entries, never
                        # entries added in the same layer, regardless of tar order.
                        # These immutable producer paths require no deletions;
                        # refuse them rather than approximate filesystem merging.
                        require(
                            target
                            and not any(
                                target == root
                                or target.startswith(root + "/")
                                or root.startswith(target + "/")
                                for root in managed_roots
                            ),
                            "Managed image whiteouts are unsupported",
                        )
                        continue
                    managed = name == BINARY or name.startswith(
                        (BRIDGE_PREFIX, PROVENANCE_PREFIX, INSTALLER_PREFIX, OPERATOR_PREFIX)
                    )
                    if any(
                        prefix.startswith(name + "/")
                        for prefix in (
                            BINARY,
                            BRIDGE_PREFIX,
                            PROVENANCE_PREFIX,
                            INSTALLER_PREFIX,
                            OPERATOR_PREFIX,
                        )
                    ):
                        require(entry.isdir(), "Managed image parent is not a directory")
                    if not managed or entry.isdir():
                        if entry.isdir():
                            payload.pop(name, None)
                        continue
                    require(entry.isfile(), "Managed image payload contains a link or special file")
                    stream = contents.extractfile(entry)
                    payload[name] = {"bytes": entry.size, "sha256": stream_digest(stream)}
        return {
            "oci_index_digest": root["digest"]
            if root["mediaType"] in (OCI_INDEX, DOCKER_INDEX)
            else None,
            "oci_manifest_digest": manifest["digest"],
            "oci_config_digest": node["config"]["digest"],
            "layer_digests": [item["digest"] for item in node["layers"]],
            "config": config,
            "payload": payload,
        }


def check_pair(root: Path, image: dict) -> tuple[dict, dict, dict]:
    """Compare the exported bridge/provenance to literal files in verified image layers."""
    bridge = bridge_info(root / "native" / "Sunny")
    provenance = json.loads((root / "provenance" / "build.json").read_text())
    lock_path = root / "provenance" / "build-inputs.json"
    lock = json.loads(lock_path.read_text())
    require(lock["build_inputs_schema_version"] == 1, "Unknown build-inputs schema")
    require(
        bool(REVISION.fullmatch(provenance["source_revision"])),
        "Release needs a concrete source revision",
    )
    require(provenance["source_date_epoch"] > 0, "Release needs a concrete source commit time")
    require(provenance["build_inputs_sha256"] == sha256(lock_path), "Build input lock mismatch")
    require(provenance["bridge"] == bridge, "Native bridge differs from build provenance")
    require(
        provenance["configuration_schema_version"] == lock["configuration_schema_version"]
        and provenance["configuration_contract"] == lock["configuration_contract"],
        "Configuration contract mismatch",
    )
    identity = configuration_identity(lock)
    if identity["configuration_schema_version"] is not None:
        compiled = provenance.get("compiled_configuration_consumer", {})
        require(
            isinstance(compiled, dict)
            and type(compiled.get("configuration_schema_version")) is int
            and compiled == identity,
            "Missing or mismatched compiled configuration consumer evidence",
        )
        require(
            native_configuration_contract(root / "native" / "Sunny") == identity,
            "Native configuration consumer differs from build contract",
        )
    for name in ("json", "pugixml"):
        expected = lock["dependencies"][name]
        require(
            provenance["dependencies"][name]
            == {**expected, "actual_revision": expected["revision"]},
            "Dependency provenance mismatch: " + name,
        )
    require(
        package_inventory(root / "provenance" / "builder-packages.tsv", lock, "builder")
        == provenance["builder_packages"],
        "Builder package inventory mismatch",
    )
    package_inventory(root / "provenance" / "runtime-packages.tsv", lock, "runtime")
    for stage in ("builder", "runtime"):
        apt_metadata(root / "provenance" / (stage + "-apt-metadata.txt"), lock, stage)
    require(
        (root / "installer" / "windows" / "Sunny.ps1").is_file(),
        "Missing Windows lifecycle installer",
    )
    require((root / "operator" / "doctor.py").is_file(), "Missing operator doctor CLI")
    payload = {}
    for prefix, directory in (
        (BRIDGE_PREFIX, root / "native" / "Sunny"),
        (PROVENANCE_PREFIX, root / "provenance"),
        (INSTALLER_PREFIX, root / "installer"),
        (OPERATOR_PREFIX, root / "operator"),
    ):
        for path in directory.rglob("*"):
            if not path.is_file() or path == root / "provenance" / "buildkit-metadata.json":
                continue
            require(not path.is_symlink(), "Export contains a symbolic link")
            payload[prefix + path.relative_to(directory).as_posix()] = {
                "sha256": sha256(path),
                "bytes": path.stat().st_size,
            }
    require(
        {name: info for name, info in image["payload"].items() if name != BINARY} == payload,
        "Native/provenance files differ from exact OCI image",
    )
    require(
        image["payload"][BINARY]["sha256"] == provenance["binary_sha256"],
        "Binary provenance mismatch",
    )
    base_layers = lock["base_image"]["layer_digests"]
    require(
        image["layer_digests"][: len(base_layers)] == base_layers,
        "OCI base layers differ from input lock",
    )
    config = image["config"]
    labels = config["config"].get("Labels") or {}
    require(
        labels.get("org.opencontainers.image.revision") == provenance["source_revision"]
        and labels.get("org.opencontainers.image.version") == provenance["product_version"]
        and labels.get("org.sunny.build-inputs.sha256") == sha256(lock_path)
        and labels.get("org.opencontainers.image.base.digest")
        == lock["base_image"]["manifest_digest"]
        and labels.get("org.sunny.configuration.contract") == lock["configuration_contract"],
        "OCI labels differ from build provenance",
    )
    if identity["configuration_schema_version"] is not None:
        require(
            labels.get("org.sunny.configuration.schema-version")
            == str(identity["configuration_schema_version"]),
            "OCI configuration schema label differs from consumer",
        )
        legacy = {
            "SUNNY_ABLETON_HOST",
            "SUNNY_TCP_PORT",
            "SUNNY_WORKSPACE_PATH",
            "SUNNY_WORKSPACE_RECOVERY",
            "SUNNY_BIND_HOST",
        }
        require(
            not any(item.split("=", 1)[0] in legacy for item in config["config"].get("Env", [])),
            "Production image bakes ambiguous legacy runtime settings",
        )
    require(config["config"].get("User") == "sunny", "Unexpected runtime identity")
    require(config["config"].get("Entrypoint") == ["sunny-mcp"], "Unexpected runtime entry point")
    return bridge, provenance, lock


def file_inventory(root: Path) -> list[dict]:
    """Record every payload file and reject symlinks or unexpected special paths."""
    files = []
    for path in sorted(root.rglob("*")):
        require(not path.is_symlink(), "Release contains a symbolic link")
        if path.is_dir() or path == root / "release.json":
            continue
        require(path.is_file(), "Release contains a special file")
        files.append(
            {
                "path": path.relative_to(root).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
        )
    return files


def export_image(image_id: str, output: Path, metadata: Path | None = None) -> dict:
    """Export an exact local image through a stopped temporary container and archive."""
    require(bool(DIGEST.fullmatch(image_id)), "Use a complete immutable local image ID")
    inspect = json.loads(run("docker", "image", "inspect", image_id))[0]
    require(inspect["Id"] == image_id, "Docker resolved a different image")
    require(
        inspect["Os"] == "linux" and inspect["Architecture"] == "amd64",
        "Unsupported release platform",
    )
    require(not output.exists(), "Output exists; preserve it and choose a new directory")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".sunny-release-", dir=output.parent) as directory:
        temporary = Path(directory)
        (temporary / "native").mkdir()
        container = run("docker", "create", "--network", "none", image_id)
        try:
            run(
                "docker",
                "cp",
                container + ":/opt/sunny/remote-script/Sunny",
                str(temporary / "native" / "Sunny"),
            )
            run("docker", "cp", container + ":/opt/sunny/release", str(temporary / "provenance"))
            run("docker", "cp", container + ":/opt/sunny/installer", str(temporary / "installer"))
            run("docker", "cp", container + ":/opt/sunny/operator", str(temporary / "operator"))
        finally:
            run("docker", "rm", container)
        run(
            "docker",
            "image",
            "save",
            "--output",
            str(temporary / "image.tar"),
            image_id,
            timeout=300,
        )
        image = archive_info(temporary / "image.tar")
        bridge, provenance, lock = check_pair(temporary, image)
        require(
            image_id
            in (
                image["oci_index_digest"],
                image["oci_manifest_digest"],
                image["oci_config_digest"],
            ),
            "Local image ID is absent from OCI archive",
        )
        if metadata:
            buildkit = json.loads(metadata.read_text())
            check_buildkit_metadata(buildkit, image)
            shutil.copyfile(metadata, temporary / "provenance" / "buildkit-metadata.json")
        manifest = {
            "release_manifest_schema_version": 1,
            "product": {"name": "Sunny", "version": provenance["product_version"]},
            "source": {
                "revision": provenance["source_revision"],
                "source_date_epoch": provenance["source_date_epoch"],
            },
            "configuration_schema_version": lock["configuration_schema_version"],
            "configuration_contract": lock["configuration_contract"],
            "bridge": bridge,
            "build_inputs_sha256": provenance["build_inputs_sha256"],
            "image": {
                "platform": "linux/amd64",
                "local_immutable_id": image_id,
                **{
                    name: image[name]
                    for name in ("oci_index_digest", "oci_manifest_digest", "oci_config_digest")
                },
                "registry_reference": None,
                "registry_manifest_digest": None,
                "archive": "image.tar",
                "binary_sha256": provenance["binary_sha256"],
            },
            "qualification": {
                "native_live": "pending",
                "production_configuration": "pending",
                "registry_publication": "not_authorized",
            },
            "files": file_inventory(temporary),
        }
        write_json(temporary / "release.json", manifest)
        temporary.rename(output)
    return manifest


def verify_release(root: Path, expected_sha256: str | None = None) -> dict:
    """Verify offline payload, OCI identities, native pairing and provenance."""
    require(not root.is_symlink(), "Release root is a symbolic link")
    manifest_path = root / "release.json"
    if expected_sha256:
        require(
            sha256(manifest_path) == expected_sha256,
            "Release manifest differs from trusted checksum",
        )
    manifest = json.loads(manifest_path.read_text())
    require(manifest["release_manifest_schema_version"] == 1, "Unknown release manifest schema")
    require(manifest["files"] == file_inventory(root), "Release file checksum/inventory mismatch")
    image = archive_info(root / "image.tar")
    bridge, provenance, lock = check_pair(root, image)
    require(manifest["bridge"] == bridge, "Manifest bridge mismatch")
    require(
        manifest["product"] == {"name": "Sunny", "version": provenance["product_version"]},
        "Manifest product mismatch",
    )
    require(
        manifest["source"]
        == {
            "revision": provenance["source_revision"],
            "source_date_epoch": provenance["source_date_epoch"],
        },
        "Manifest source mismatch",
    )
    require(
        manifest["build_inputs_sha256"] == provenance["build_inputs_sha256"],
        "Manifest build lock mismatch",
    )
    require(
        manifest["configuration_schema_version"] == lock["configuration_schema_version"]
        and manifest["configuration_contract"] == lock["configuration_contract"],
        "Manifest configuration mismatch",
    )
    expected_image = manifest["image"]
    require(
        expected_image["platform"] == "linux/amd64" and expected_image["archive"] == "image.tar",
        "Manifest image contract mismatch",
    )
    for name in ("oci_index_digest", "oci_manifest_digest", "oci_config_digest"):
        require(expected_image[name] == image[name], "Manifest OCI identity mismatch: " + name)
    require(
        expected_image["local_immutable_id"]
        in (image["oci_index_digest"], image["oci_manifest_digest"], image["oci_config_digest"]),
        "Manifest local ID differs from archive",
    )
    require(
        expected_image["binary_sha256"] == provenance["binary_sha256"], "Manifest binary mismatch"
    )
    require(
        expected_image["registry_reference"] is None
        and expected_image["registry_manifest_digest"] is None,
        "This offline producer has no verified registry publication",
    )
    metadata = root / "provenance" / "buildkit-metadata.json"
    if metadata.exists():
        buildkit = json.loads(metadata.read_text())
        check_buildkit_metadata(buildkit, image)
    require(
        manifest["qualification"]
        == {
            "native_live": "pending",
            "production_configuration": "pending",
            "registry_publication": "not_authorized",
        },
        "This producer has no native qualification or publication approval",
    )
    return manifest


def build_release(output: Path, tag: str) -> dict:
    """Build one clean concrete revision with closed inputs and export its actual image."""
    require(not output.exists(), "Output already exists")
    require(
        not run("git", "status", "--porcelain", "--untracked-files=all", cwd=ROOT),
        "Release build requires a clean committed source checkout",
    )
    revision = run("git", "rev-parse", "HEAD", cwd=ROOT)
    require(bool(REVISION.fullmatch(revision)), "Release source revision is not concrete")
    epoch = run("git", "show", "-s", "--format=%ct", revision, cwd=ROOT)
    lock = json.loads(LOCK.read_text())
    with tempfile.TemporaryDirectory(prefix="sunny-release-build-") as directory:
        metadata = Path(directory) / "buildkit-metadata.json"
        arguments = [
            "docker",
            "buildx",
            "build",
            "--platform",
            lock["release_platform"],
            "--load",
            "--metadata-file",
            str(metadata),
            "--tag",
            tag,
        ]
        inputs = {
            "SUNNY_APT_SNAPSHOT": lock["ubuntu"]["snapshot"],
            "SUNNY_CA_CERTIFICATES_URL": lock["ubuntu"]["snapshot_service"]
            + lock["ubuntu"]["snapshot"]
            + "/"
            + lock["ubuntu"]["packages"]["ca-certificates"]["filename"],
            "SUNNY_CA_CERTIFICATES_SHA256": lock["ubuntu"]["packages"]["ca-certificates"]["sha256"],
            "SUNNY_SOURCE_REVISION": revision,
            "SUNNY_VERSION": json.loads(
                (ROOT / "pyproject.toml").read_text().split("version = ", 1)[1].splitlines()[0]
            ),
            "SUNNY_BUILD_INPUTS_SHA256": sha256(LOCK),
            "SOURCE_DATE_EPOCH": epoch,
        }
        for stage in ("builder", "runtime"):
            key = "SUNNY_BUILD_PACKAGES" if stage == "builder" else "SUNNY_RUNTIME_PACKAGES"
            inputs[key] = " ".join(
                name + "=" + lock["ubuntu"]["packages"][name]["version"]
                for name in lock["ubuntu"][stage + "_packages"]
            )
        for name, value in inputs.items():
            arguments += ["--build-arg", name + "=" + value]
        # Read only committed tree bytes; concurrent worktree edits cannot silently
        # change the content attributed to this concrete revision.
        context = Path(directory) / "source"
        context.mkdir()
        source_tar = Path(directory) / "source.tar"
        run("git", "archive", "--format=tar", "--output=" + str(source_tar), revision, cwd=ROOT)
        with tarfile.open(source_tar) as source:
            for entry in source.getmembers():
                require(
                    not entry.name.startswith("/") and ".." not in PurePosixPath(entry.name).parts,
                    "Unsafe source archive path",
                )
                require(
                    entry.isdir() or entry.isfile(),
                    "Release source must contain tracked regular files",
                )
            if hasattr(tarfile, "data_filter"):
                source.extractall(context, filter="data")
            else:
                # All members above are regular tracked files or directories,
                # including on Python 3.10 before extraction filters existed.
                source.extractall(context)
        committed_lock = context / "release" / "build-inputs.json"
        require(
            sha256(committed_lock) == inputs["SUNNY_BUILD_INPUTS_SHA256"],
            "Build input lock changed after clean-source check",
        )
        arguments.append(str(context))
        run(*arguments, timeout=3600)
        buildkit = json.loads(metadata.read_text())
        digest = buildkit.get("containerimage.digest")
        require(
            isinstance(digest, str) and DIGEST.fullmatch(digest), "Missing BuildKit image digest"
        )
        config = buildkit.get("containerimage.config.digest")
        if "containerimage.config.digest" in buildkit:
            require(
                isinstance(config, str) and DIGEST.fullmatch(config),
                "Invalid BuildKit config digest",
            )
        image = json.loads(run("docker", "image", "inspect", tag))[0]
        require(
            image["Id"] in (digest, config)
            or (image.get("Descriptor") or {}).get("digest") == digest,
            "Tag changed after build; export the immutable build result",
        )
        return export_image(image["Id"], output, metadata)


def load_release(root: Path, expected_sha256: str | None) -> dict:
    """Import verified bytes and report the destination store's actual immutable local ID."""
    manifest = verify_release(root, expected_sha256)
    run("docker", "image", "load", "--input", str(root / "image.tar"), timeout=300)
    identities = [
        manifest["image"][name]
        for name in ("local_immutable_id", "oci_config_digest", "oci_manifest_digest")
    ]
    image = None
    for identity in dict.fromkeys(identities):
        try:
            image = json.loads(run("docker", "image", "inspect", identity))[0]
            break
        except subprocess.CalledProcessError:
            continue
    require(image is not None, "Loaded image did not expose any verified immutable identity")
    require(
        image["Os"] == "linux" and image["Architecture"] == "amd64",
        "Loaded image platform mismatch",
    )
    require(
        (image["Config"].get("Labels") or {}).get("org.opencontainers.image.revision")
        == manifest["source"]["revision"],
        "Loaded image revision mismatch",
    )
    return {
        "loaded_image_local_immutable_id": image["Id"],
        "oci_manifest_digest": manifest["image"]["oci_manifest_digest"],
        "release_manifest_sha256": sha256(root / "release.json"),
    }


def main() -> None:
    """Dispatch finite local release operations; publication is intentionally absent."""
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser(
        "build", help="Build a clean committed checkout and export paired artifacts"
    )
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--tag", default="sunny-mcp:local-release")
    export = commands.add_parser("export", help="Export an existing immutable release image")
    export.add_argument("--image-id", required=True)
    export.add_argument("--output", type=Path, required=True)
    for name in ("verify", "load"):
        command = commands.add_parser(name)
        command.add_argument("--release", type=Path, required=True)
        command.add_argument(
            "--expected-manifest-sha256", help="Trusted checksum obtained separately"
        )
    record = commands.add_parser(
        "record-build", help="Capture builder provenance (used by Dockerfile)"
    )
    record.add_argument("--source-revision", required=True)
    record.add_argument("--source-date-epoch", required=True, type=int)
    record.add_argument("--build-directory", required=True, type=Path)
    record.add_argument("--packages", required=True, type=Path)
    record.add_argument("--apt-metadata", required=True, type=Path)
    record.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.command == "record-build":
        record_build(args)
        return
    if args.command == "build":
        manifest = build_release(args.output, args.tag)
    elif args.command == "export":
        manifest = export_image(args.image_id, args.output)
    elif args.command == "verify":
        manifest = verify_release(args.release, args.expected_manifest_sha256)
    else:
        print(json.dumps(load_release(args.release, args.expected_manifest_sha256), indent=2))
        return
    root = args.output if args.command in ("build", "export") else args.release
    print(
        json.dumps(
            {
                "release": str(root),
                "release_manifest_sha256": sha256(root / "release.json"),
                "manifest": manifest,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, tarfile.TarError, subprocess.SubprocessError) as error:
        print("Release operation failed: " + str(error), file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr[-8000:], file=sys.stderr)
        sys.exit(1)
