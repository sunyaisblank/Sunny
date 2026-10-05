"""Read-only artifact identity; no Docker daemon, MCP startup or Live connection."""

import argparse
import hashlib
import json
import re
from pathlib import Path
from types import ModuleType


def sha(path):
    """Hash actual file bytes in bounded chunks."""
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def bridge_sha(root):
    """Hash the literal versioned stream of exported native module bytes."""
    digest = hashlib.sha256(b"sunny-remote-script-v1\n")
    paths = sorted(Path(root).rglob("*.py"), key=lambda p: p.relative_to(root).as_posix())
    for path in paths:
        digest.update((path.relative_to(root).as_posix() + "\n" + sha(path) + "\n").encode("ascii"))
    return digest.hexdigest(), len(paths)


def verify_paired_release(root, trusted_sha256):
    """Verify the full paired OCI release without executing its bridge or opening sockets."""
    if not re.fullmatch(r"[0-9a-f]{64}", trusted_sha256 or ""):
        raise ValueError("Supply the separately retained lowercase release manifest SHA256")
    root = Path(root)
    manifest = root / "release.json"
    if manifest.is_symlink() or not manifest.is_file() or manifest.stat().st_size > 1024 * 1024:
        raise ValueError("Supply a bounded physical release manifest")
    if sha(manifest) != trusted_sha256:
        raise ValueError("Release manifest differs from the separately retained checksum")
    authenticated = json.loads(manifest.read_text(encoding="utf-8"))
    # Both the source checkout and the exported operator directory place the
    # independently reviewed release verifier beside this kit directory.
    verifier = Path(__file__).resolve().parent.parent / "release.py"
    files = authenticated.get("files")
    if not isinstance(files, list):
        raise ValueError("Release has no operator inventory")
    # Authenticate the delegate BEFORE importing it. The older candidate mode
    # stays available; full release mode requires this packaged operator unit.
    delegate_bytes = None
    for relative, actual in (
        ("operator/release.py", verifier),
        ("operator/live_qualification/verify_artifacts.py", Path(__file__)),
    ):
        matches = [
            item for item in files if isinstance(item, dict) and item.get("path") == relative
        ]
        if len(matches) != 1 or set(matches[0]) != {"path", "bytes", "sha256"}:
            raise ValueError("Release lacks the exact packaged operator: " + relative)
        item = matches[0]
        if (
            actual.is_symlink()
            or not actual.is_file()
            or type(item["bytes"]) is not int
            or item["bytes"] != actual.stat().st_size
            or not 0 < item["bytes"] <= 1024 * 1024
        ):
            raise ValueError("Operator bytes differ from the authenticated release: " + relative)
        data = actual.read_bytes()
        if len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError("Operator bytes differ from the authenticated release: " + relative)
        if relative == "operator/release.py":
            delegate_bytes = data
    # Execute precisely the authenticated in-memory source. Import loaders can
    # create __pycache__ inside the release and invalidate its physical inventory;
    # rereading a file after checking its hash could execute different bytes.
    module = ModuleType("sunny_qualification_release")
    module.__file__ = str(verifier)
    exec(compile(delegate_bytes, str(verifier), "exec"), module.__dict__)
    expected = module.verify_release(root, trusted_sha256)
    bundled = root / "operator" / "live_qualification"
    return {
        "release_manifest_sha256": trusted_sha256,
        "source_revision": expected["source"]["revision"],
        "bridge": expected["bridge"],
        "image": expected["image"],
        "configuration_schema_version": expected["configuration_schema_version"],
        "toolkit_in_release": bundled.is_dir(),
        "host_executed": False,
        "qualification": expected["qualification"],
    }


def main():
    """Select full paired-release verification or the retained original candidate mode."""
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--release", help="Full verified release directory including image.tar")
    mode.add_argument("--candidate", help="Retained original candidate metadata")
    parser.add_argument("--expected-manifest-sha256")
    parser.add_argument("--archive")
    parser.add_argument("--sunny-directory")
    parser.add_argument(
        "--binary", help="Optional extracted actual container binary, not a different local build"
    )
    args = parser.parse_args()
    if args.release:
        if args.archive or args.sunny_directory or args.binary:
            parser.error("Legacy candidate arguments cannot accompany --release")
        actual = verify_paired_release(args.release, args.expected_manifest_sha256)
        print(json.dumps(actual, indent=2))
        return
    if args.expected_manifest_sha256 or not args.archive or not args.sunny_directory:
        parser.error("Candidate mode requires --archive and --sunny-directory")
    expected = json.loads(Path(args.candidate).read_text())
    bridge, count = bridge_sha(Path(args.sunny_directory))
    actual = {
        "archive_sha256": sha(args.archive),
        "bridge_source_sha256": bridge,
        "python_modules": count,
    }
    if actual["archive_sha256"] != expected["image_archive_sha256"]:
        raise ValueError("Archive checksum mismatch")
    if bridge != expected["bridge_source_sha256"]:
        raise ValueError("Sunny bridge bytes mismatch")
    if args.binary:
        actual["mcp_binary_sha256"] = sha(args.binary)
        if actual["mcp_binary_sha256"] != expected["mcp_binary_sha256"]:
            raise ValueError("MCP binary checksum mismatch")
    print(
        json.dumps(
            {"expected_candidate": expected, "actual": actual, "host_executed": False}, indent=2
        )
    )


if __name__ == "__main__":
    main()
