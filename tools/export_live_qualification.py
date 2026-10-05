"""Package the full offline release and its qualification kit from one immutable image.

The image must already contain the maintained kit. No unrelated checkout kit is
substituted, image published, container started, or native host modified.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
import zipfile
from pathlib import Path

sys.dont_write_bytecode = True

import release  # noqa: E402 - suppress caches before importing verified release helpers.


def package(root: Path, output: Path) -> dict:
    """Preserve the exact paired inventory; create a new deterministic transport ZIP."""
    manifest_sha256 = hashlib.sha256((root / "release.json").read_bytes()).hexdigest()
    manifest = release.verify_release(root, manifest_sha256)
    for name in (
        "operator/doctor.py",
        "operator/release.py",
        "operator/live_qualification/common.py",
        "operator/live_qualification/host_runner.py",
        "operator/live_qualification/verify_artifacts.py",
        "operator/live_qualification/SunnyHostProbe/probe.py",
    ):
        if not (root / name).is_file():
            raise ValueError("This image lacks the paired qualification helper: " + name)
    created = False
    try:
        with output.open("xb") as stream:
            created = True
            with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_DEFLATED) as archive:
                for path in sorted(root.rglob("*")):
                    if path.is_dir():
                        continue
                    if not path.is_file() or path.is_symlink():
                        raise ValueError("Release contains a nonphysical file")
                    info = zipfile.ZipInfo(
                        "release/" + path.relative_to(root).as_posix(),
                        date_time=(1980, 1, 1, 0, 0, 0),
                    )
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    archive.writestr(info, path.read_bytes())
    except BaseException:
        if created:
            output.unlink()
        raise
    return {
        "output": str(output),
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "release_manifest_sha256": manifest_sha256,
        "image_local_immutable_id": manifest["image"]["local_immutable_id"],
        "host_executed": False,
    }


def main() -> None:
    """Use the release producer to export the image, bridge and all operator bytes together."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--image-id", required=True, help="Complete immutable local sha256: image ID"
    )
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists; preserve it and select a new filename")
    with tempfile.TemporaryDirectory(prefix="sunny-qualification-export-") as directory:
        root = Path(directory) / "release"
        release.export_image(args.image_id, root)
        report = package(root, args.output)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
