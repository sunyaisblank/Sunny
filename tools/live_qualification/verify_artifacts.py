"""Read-only artifact identity; no Docker daemon, MCP startup or Live connection."""

import argparse
import hashlib
import json
from pathlib import Path


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


def main():
    """Run only the explicitly selected command-line qualification operation."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--sunny-directory", required=True)
    parser.add_argument(
        "--binary", help="Optional extracted actual container binary, not a different local build"
    )
    args = parser.parse_args()
    expected = json.loads(Path(args.candidate).read_text())
    bridge, count = bridge_sha(Path(args.sunny_directory))
    actual = {
        "archive_sha256": sha(args.archive),
        "bridge_source_sha256": bridge,
        "python_modules": count,
    }
    assert actual["archive_sha256"] == expected["image_archive_sha256"], "Archive checksum mismatch"
    assert bridge == expected["bridge_source_sha256"], "Sunny bridge bytes mismatch"
    if args.binary:
        actual["mcp_binary_sha256"] = sha(args.binary)
        assert actual["mcp_binary_sha256"] == expected["mcp_binary_sha256"], (
            "MCP binary checksum mismatch"
        )
    print(
        json.dumps(
            {"expected_candidate": expected, "actual": actual, "host_executed": False}, indent=2
        )
    )


if __name__ == "__main__":
    main()
