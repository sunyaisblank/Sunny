"""Export the maintained qualification kit with the bridge from one immutable image.

This produces a transport artifact, not a qualified production release. The
image must already be present. No image is built/pulled/published, no server is
started, and no host installation or security setting is changed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
import zipfile
from pathlib import Path

KIT = Path(__file__).resolve().parent / "live_qualification"
IDENTITY = re.compile(r"sha256:[0-9a-f]{64}\Z")


def docker(*arguments: str) -> str:
    """Run one finite Docker command without a shell or credential arguments."""
    result = subprocess.run(
        ["docker", *arguments], capture_output=True, text=True, timeout=60, check=True
    )
    return result.stdout.strip()


def bridge_identity(root: Path) -> str:
    """Hash the literal versioned module stream independently of the deployed loader."""
    digest = hashlib.sha256(b"sunny-remote-script-v1\n")
    for path in sorted(root.rglob("*.py")):
        name = path.relative_to(root).as_posix()
        if not re.fullmatch(r"(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.py", name):
            raise ValueError("Unsupported exported source path: " + name)
        digest.update((name + "\n" + hashlib.sha256(path.read_bytes()).hexdigest() + "\n").encode())
    return digest.hexdigest()


def package(bridge: Path, image: dict, output: Path) -> dict:
    """Write a new deterministic archive; refuse existing output or wrong bridge bytes."""
    contract = json.loads((bridge / "bridge_contract.json").read_text())
    if contract != {"bridge_protocol_version": 46, "target_snapshot_schema_version": 35}:
        raise ValueError("Qualification requests require reviewed protocol46/snapshot35")
    source_hash = bridge_identity(bridge)
    if (bridge / "source.sha256").read_text().strip() != source_hash:
        raise ValueError("Exported bridge source checksum does not match its module bytes")
    files = {}
    for root, prefix in ((KIT, "qualification"), (bridge, "native/Sunny")):
        for path in sorted(root.rglob("*")):
            if not path.is_file() or "__pycache__" in path.parts or path.suffix == ".pyc":
                continue
            files[prefix + "/" + path.relative_to(root).as_posix()] = path.read_bytes()
    config_path = "qualification/configuration.json"
    config = json.loads(files[config_path])
    config.update(
        image_local_immutable_id=image["Id"],
        expected_bridge_source_sha256=source_hash,
        expected_bridge_protocol_version=contract["bridge_protocol_version"],
        expected_target_snapshot_schema_version=contract["target_snapshot_schema_version"],
    )
    files[config_path] = (json.dumps(config, indent=2) + "\n").encode()
    manifest = {
        "qualification_package_schema_version": 1,
        "host_executed": False,
        "image_local_immutable_id": image["Id"],
        "image_os": image["Os"],
        "image_architecture": image["Architecture"],
        "image_registry_digests": image.get("RepoDigests", []),
        "image_source_revision": (image["Config"].get("Labels") or {}).get(
            "org.opencontainers.image.revision"
        ),
        "bridge_source_sha256": source_hash,
        "bridge_contract": contract,
        "files": [
            {"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            for name, data in sorted(files.items())
        ],
    }
    files["package.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    # Exclusive creation preserves earlier packages. Failed writes never leave a
    # plausible partial ZIP; no install uses this output before verification.
    created = False
    try:
        with output.open("xb") as stream:
            created = True
            with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for name, data in sorted(files.items()):
                    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    archive.writestr(info, data)
    except BaseException:
        if created:
            output.unlink()
        raise
    return manifest


def main() -> None:
    """Export from an unstarted temporary container and remove only that container."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-id", required=True, help="Full local sha256: image identity")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if not IDENTITY.fullmatch(args.image_id):
        parser.error("Use the complete immutable sha256: image ID, not a tag")
    if args.output.exists():
        parser.error("Output already exists; preserve it and choose a new filename")
    image = json.loads(docker("image", "inspect", args.image_id))[0]
    if image["Id"] != args.image_id:
        raise ValueError("Docker did not resolve the exact requested immutable image")
    with tempfile.TemporaryDirectory(prefix="sunny-qualification-export-") as directory:
        root = Path(directory)
        container = docker("create", "--network", "none", args.image_id)
        try:
            docker("cp", container + ":/opt/sunny/remote-script/Sunny", str(root / "Sunny"))
        finally:
            docker("rm", container)
        manifest = package(root / "Sunny", image, args.output)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    print(
        json.dumps({"output": str(args.output), "sha256": digest, "manifest": manifest}, indent=2)
    )


if __name__ == "__main__":
    main()
