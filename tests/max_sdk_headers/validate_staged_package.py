"""Validate the exact authored and binary topology of a staged Sunny Max package."""

from __future__ import annotations

import argparse
import hashlib
import zipfile
from pathlib import Path, PurePosixPath

OBJECTS = ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events")


def _exact_children(directory: Path, expected: set[str]) -> None:
    """Require one directory to contain exactly the declared child names."""
    if not directory.is_dir():
        raise ValueError(f"missing staged directory: {directory}")
    observed = {path.name for path in directory.iterdir()}
    if observed != expected:
        missing = sorted(expected - observed)
        unexpected = sorted(observed - expected)
        raise ValueError(
            f"staged directory mismatch at {directory}: missing={missing}, unexpected={unexpected}"
        )


def _require_identical_regular_file(source: Path, staged: Path) -> None:
    """Require an authored file to be copied byte-for-byte without a symlink leaf."""
    if source.is_symlink() or not source.is_file():
        raise ValueError(f"invalid authored package file: {source}")
    if staged.is_symlink() or not staged.is_file():
        raise ValueError(f"missing or non-regular staged file: {staged}")
    if source.read_bytes() != staged.read_bytes():
        raise ValueError(f"staged authored file diverges from source: {staged}")


def _validate_archive(staged: Path, archive: Path) -> None:
    """Require one regular ZIP and digest sidecar to reproduce every staged file byte."""
    if archive.is_symlink() or not archive.is_file() or archive.stat().st_size == 0:
        raise ValueError(f"missing, empty, or non-regular Max package archive: {archive}")
    staged_files: dict[str, Path] = {}
    for path in staged.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"staged package contains a symlink: {path}")
        if path.is_file():
            staged_files[f"Sunny/{path.relative_to(staged).as_posix()}"] = path

    try:
        with zipfile.ZipFile(archive) as zipped:
            if zipped.testzip() is not None:
                raise ValueError(f"Max package archive has an invalid CRC: {archive}")
            entries = [entry for entry in zipped.infolist() if not entry.is_dir()]
            names = [entry.filename for entry in entries]
            if len(names) != len(set(names)):
                raise ValueError(f"Max package archive contains duplicate paths: {archive}")
            for entry in entries:
                path = PurePosixPath(entry.filename)
                unix_type = (entry.external_attr >> 16) & 0o170000
                if (
                    path.is_absolute()
                    or "\\" in entry.filename
                    or any(component in {"", ".", ".."} for component in path.parts)
                    or unix_type == 0o120000
                ):
                    raise ValueError(f"unsafe Max package archive entry: {entry.filename}")
            if set(names) != set(staged_files):
                missing = sorted(set(staged_files) - set(names))
                unexpected = sorted(set(names) - set(staged_files))
                raise ValueError(
                    f"Max package archive topology mismatch: "
                    f"missing={missing}, unexpected={unexpected}"
                )
            for entry in entries:
                if zipped.read(entry) != staged_files[entry.filename].read_bytes():
                    raise ValueError(
                        f"Max package archive bytes diverge from stage: {entry.filename}"
                    )
    except zipfile.BadZipFile as error:
        raise ValueError(f"invalid Max package ZIP: {archive}") from error

    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    sidecar = Path(f"{archive}.sha256")
    expected_sidecar = f"{digest}  {archive.name}\n"
    if sidecar.is_symlink() or not sidecar.is_file():
        raise ValueError(f"missing or non-regular Max package digest sidecar: {sidecar}")
    if sidecar.read_text(encoding="ascii") != expected_sidecar:
        raise ValueError(f"Max package digest sidecar diverges from archive: {sidecar}")


def validate_staged_package(source: Path, staged: Path, archive: Path, platform: str) -> None:
    """Validate one platform package after the pinned Max SDK staging build."""
    if platform not in {"macos", "windows"}:
        raise ValueError(f"unsupported Max package platform: {platform}")
    source = source.resolve(strict=True)
    staged = staged.resolve(strict=True)

    authored = [Path("package-info.json"), Path("readme.md")]
    authored.extend(Path("docs") / f"{name}.maxref.xml" for name in OBJECTS)
    authored.extend(Path("help") / f"{name}.maxhelp" for name in OBJECTS)
    authored.extend(
        Path("patchers") / name
        for name in (
            "sunny-runtime-smoke.maxtest.maxpat",
            "sunny.assert-host-status.maxpat",
            "sunny.assert-event-status.maxpat",
            "sunny.assert-event-list.maxpat",
            "sunny.assert-host-lifecycle.maxpat",
        )
    )
    authored.extend(
        (
            Path("misc/validation/README.md"),
            Path("misc/validation/max-host-run-plan.json"),
            Path("misc/validation/max-validation-record.example.json"),
            Path("misc/validation/max-validation-observation.example.json"),
            Path("misc/validation/max-release-matrix.json"),
            Path("misc/validation/max-test-harness.json"),
            Path("misc/validation/export-max-test-sqlite.py"),
            Path("misc/validation/export-m4l-assertions.py"),
            Path("misc/validation/index-max-host-run.py"),
            Path("misc/validation/prepare-m4l-device-source.py"),
            Path("misc/validation/prepare-max-host-run.py"),
            Path("misc/validation/run-named-live-validation.py"),
        )
    )
    for relative in authored:
        _require_identical_regular_file(source / relative, staged / relative)

    _exact_children(staged / "docs", {f"{name}.maxref.xml" for name in OBJECTS})
    _exact_children(staged / "help", {f"{name}.maxhelp" for name in OBJECTS})
    _exact_children(
        staged / "patchers",
        {
            "sunny-runtime-smoke.maxtest.maxpat",
            "sunny.assert-host-status.maxpat",
            "sunny.assert-event-status.maxpat",
            "sunny.assert-event-list.maxpat",
            "sunny.assert-host-lifecycle.maxpat",
        },
    )
    _exact_children(
        staged / "misc" / "validation",
        {
            "README.md",
            "max-host-run-plan.json",
            "max-validation-record.example.json",
            "max-validation-observation.example.json",
            "max-release-matrix.json",
            "max-test-harness.json",
            "export-max-test-sqlite.py",
            "export-m4l-assertions.py",
            "index-max-host-run.py",
            "prepare-m4l-device-source.py",
            "prepare-max-host-run.py",
            "run-named-live-validation.py",
        },
    )

    external_names = (
        {f"{name}.mxo" for name in OBJECTS}
        if platform == "macos"
        else {f"{name}.mxe64" for name in OBJECTS}
    )
    _exact_children(staged / "externals", external_names)
    for name in OBJECTS:
        binary = (
            staged / "externals" / f"{name}.mxo" / "Contents" / "MacOS" / name
            if platform == "macos"
            else staged / "externals" / f"{name}.mxe64"
        )
        if binary.is_symlink() or not binary.is_file() or binary.stat().st_size == 0:
            raise ValueError(f"missing, empty, or non-regular Max external binary: {binary}")
    _validate_archive(staged, archive)


def main() -> None:
    """Parse command-line paths and validate one staged package."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--staged", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--platform", choices=("macos", "windows"), required=True)
    arguments = parser.parse_args()
    validate_staged_package(
        arguments.source, arguments.staged, arguments.archive, arguments.platform
    )
    print(f"validated Sunny Max package for {arguments.platform}: {arguments.staged}")


if __name__ == "__main__":
    main()
