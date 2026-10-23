"""Identify the exact installed Sunny Python sources without Git or a Live host.

The fingerprint covers sorted relative module names and SHA256 of their raw
bytes. CMake uses the same versioned byte stream for the native expected value.
It establishes matching source compatibility, not native API qualification.
"""

from __future__ import annotations

import hashlib
import re
from pathlib import Path

_REQUIRED_SOURCES = (
    "__init__.py",
    "build_identity.py",
    "diagnostics.py",
    "handler.py",
    "managed.py",
    "managed_capacity.py",
    "managed_devices.py",
    "managed_geometry.py",
    "managed_envelope_revision.py",
    "managed_device_modes.py",
    "managed_recovery.py",
    "managed_song_settings.py",
    "managed_mixer.py",
    "managed_routing.py",
    "native_mixer_units.py",
    "native_mixer_preview.py",
    "native_units.py",
    "server.py",
    "surface.py",
)
_SOURCE_PATH = re.compile(r"(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+\.py\Z")


def compute_source_identity(source_root: Path) -> str:
    """Read a complete source folder and return its versioned SHA256 identity."""
    for name in _REQUIRED_SOURCES:
        if not (source_root / name).is_file():
            raise RuntimeError(
                "Sunny Remote Script is incomplete: missing {}. "
                "Install the Sunny folder exported from the matching server image.".format(
                    source_root / name
                )
            )
    sources = sorted(
        path.relative_to(source_root).as_posix()
        for path in source_root.rglob("*.py")
        if path.is_file()
    )
    manifest = "sunny-remote-script-v1\n"
    for name in sources:
        if not _SOURCE_PATH.fullmatch(name):
            raise RuntimeError("Unsupported Sunny Remote Script source path: {}".format(name))
        try:
            file_digest = hashlib.sha256((source_root / name).read_bytes()).hexdigest()
        except OSError as error:
            raise RuntimeError(
                "Cannot read Sunny Remote Script source {}: {}. "
                "Install the complete matching server image bundle.".format(
                    source_root / name, error
                )
            ) from error
        manifest += name + "\n" + file_digest + "\n"
    return hashlib.sha256(manifest.encode("ascii")).hexdigest()


BRIDGE_SOURCE_SHA256 = compute_source_identity(Path(__file__).resolve().parent)
