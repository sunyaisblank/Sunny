"""Tests for the cross-platform staged Max package topology gate."""

from __future__ import annotations

import hashlib
import importlib.util
import os
import shutil
import subprocess
import sys
import zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import ModuleType

import pytest


def _validator_module() -> ModuleType:
    path = Path(__file__).parents[1] / "max_sdk_headers" / "validate_staged_package.py"
    specification = importlib.util.spec_from_file_location("sunny_staged_max_validator", path)
    assert specification is not None and specification.loader is not None
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    return module


VALIDATOR = _validator_module()
OBJECTS: tuple[str, ...] = VALIDATOR.OBJECTS


def _stage_authored_package(source: Path, staged: Path, platform: str) -> None:
    staged.mkdir()
    for relative in (Path("package-info.json"), Path("readme.md")):
        shutil.copy2(source / relative, staged / relative)
    for directory in ("docs", "help"):
        shutil.copytree(source / directory, staged / directory)
    externals = staged / "externals"
    externals.mkdir()
    for name in OBJECTS:
        binary = (
            externals / f"{name}.mxo" / "Contents" / "MacOS" / name
            if platform == "macos"
            else externals / f"{name}.mxe64"
        )
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(f"binary:{name}".encode())


def _archive(staged: Path, archive: Path) -> None:
    with zipfile.ZipFile(archive, "w") as zipped:
        for path in staged.rglob("*"):
            if path.is_file():
                zipped.write(path, f"Sunny/{path.relative_to(staged).as_posix()}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    Path(f"{archive}.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")


@pytest.mark.parametrize("platform", ["macos", "windows"])
def test_staged_max_package_requires_every_binary_and_authored_file(
    tmp_path: Path, platform: str
) -> None:
    """Require the complete five-object package on both supported platform layouts."""
    source = Path(__file__).parents[2] / "max-package"
    staged = tmp_path / "package"
    archive = tmp_path / "Sunny.zip"
    _stage_authored_package(source, staged, platform)
    _archive(staged, archive)
    VALIDATOR.validate_staged_package(source, staged, archive, platform)

    missing = (
        staged / "externals" / "sunny.events.mxo"
        if platform == "macos"
        else staged / "externals" / "sunny.events.mxe64"
    )
    if missing.is_dir():
        shutil.rmtree(missing)
    else:
        missing.unlink()
    with pytest.raises(ValueError, match="staged directory mismatch"):
        VALIDATOR.validate_staged_package(source, staged, archive, platform)


def test_staged_max_package_rejects_authored_drift_and_unexpected_externals(
    tmp_path: Path,
) -> None:
    """Reject copied metadata drift and any undeclared external at the package boundary."""
    source = Path(__file__).parents[2] / "max-package"
    staged = tmp_path / "package"
    archive = tmp_path / "Sunny.zip"
    _stage_authored_package(source, staged, "windows")
    _archive(staged, archive)

    reference = staged / "docs" / "sunny.events.maxref.xml"
    reference.write_text("changed", encoding="utf-8")
    with pytest.raises(ValueError, match="diverges from source"):
        VALIDATOR.validate_staged_package(source, staged, archive, "windows")

    shutil.copy2(source / "docs" / reference.name, reference)
    (staged / "externals" / "unexpected.mxe64").write_bytes(b"unexpected")
    with pytest.raises(ValueError, match="unexpected"):
        VALIDATOR.validate_staged_package(source, staged, archive, "windows")


def test_staged_max_package_rejects_archive_and_digest_divergence(tmp_path: Path) -> None:
    """Require the published ZIP and digest sidecar to match every staged file byte."""
    source = Path(__file__).parents[2] / "max-package"
    staged = tmp_path / "package"
    archive = tmp_path / "Sunny.zip"
    _stage_authored_package(source, staged, "windows")
    _archive(staged, archive)

    (staged / "externals" / "sunny.lfo~.mxe64").write_bytes(b"new bytes")
    with pytest.raises(ValueError, match="archive bytes diverge"):
        VALIDATOR.validate_staged_package(source, staged, archive, "windows")

    _archive(staged, archive)
    Path(f"{archive}.sha256").write_text("0" * 64 + f"  {archive.name}\n", encoding="ascii")
    with pytest.raises(ValueError, match="digest sidecar diverges"):
        VALIDATOR.validate_staged_package(source, staged, archive, "windows")


def test_cmake_max_archive_target_script_produces_the_validated_bundle(tmp_path: Path) -> None:
    """Exercise the same portable archive script invoked by both platform CI jobs."""
    source = Path(__file__).parents[2] / "max-package"
    staged = tmp_path / "package"
    archive = tmp_path / "Sunny-0.4.0-windows-x86_64.zip"
    _stage_authored_package(source, staged, "windows")

    subprocess.run(
        [
            "cmake",
            f"-DSUNNY_MAX_STAGED_PACKAGE={staged}",
            f"-DSUNNY_MAX_ARCHIVE={archive}",
            "-P",
            str(source / "cmake" / "CreateArchive.cmake"),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    VALIDATOR.validate_staged_package(source, staged, archive, "windows")


def _run_archive(staged: Path, archive: Path, *, fail_archiver: bool = False):
    source = Path(__file__).parents[2] / "max-package"
    command = [
        "cmake",
        f"-DSUNNY_MAX_STAGED_PACKAGE={staged}",
        f"-DSUNNY_MAX_ARCHIVE={archive}",
    ]
    if fail_archiver:
        # An existing executable that rejects CMake's archiver arguments, on every test OS.
        command.append(f"-DCMAKE_COMMAND={sys.executable}")
    command.extend(["-P", str(source / "cmake" / "CreateArchive.cmake")])
    return subprocess.run(command, capture_output=True, text=True, timeout=30)


def test_max_archive_failure_preserves_retained_pair(tmp_path: Path) -> None:
    """A failed archiver must leave an earlier valid archive and digest intact."""
    staged = tmp_path / "package"
    staged.mkdir()
    (staged / "content.txt").write_text("original", encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    sidecar = Path(f"{archive}.sha256")
    assert _run_archive(staged, archive).returncode == 0
    before = archive.read_bytes(), sidecar.read_bytes()
    failed = _run_archive(staged, archive, fail_archiver=True)
    assert failed.returncode != 0
    assert archive.exists() and sidecar.exists()
    assert (archive.read_bytes(), sidecar.read_bytes()) == before


def test_max_archive_repeat_is_identical_and_changed_inputs_are_declined(tmp_path: Path) -> None:
    """Published bytes are immutable; an identical repeat is a successful no-op."""
    staged = tmp_path / "package"
    staged.mkdir()
    content = staged / "content.txt"
    content.write_text("original", encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    sidecar = Path(f"{archive}.sha256")
    assert _run_archive(staged, archive).returncode == 0
    before = archive.read_bytes(), sidecar.read_bytes()
    os.utime(content, (946684800, 946684800))  # Metadata alone must not change published bytes.
    assert _run_archive(staged, archive).returncode == 0
    assert (archive.read_bytes(), sidecar.read_bytes()) == before
    content.write_text("changed", encoding="utf-8")
    declined = _run_archive(staged, archive)
    assert declined.returncode != 0
    assert (archive.read_bytes(), sidecar.read_bytes()) == before


@pytest.mark.parametrize("occupied", ["archive", "sidecar"])
def test_max_archive_does_not_replace_an_incomplete_destination(
    tmp_path: Path, occupied: str
) -> None:
    """An incomplete existing pair belongs to its owner and cannot be overwritten."""
    staged = tmp_path / "package"
    staged.mkdir()
    (staged / "content.txt").write_text("new", encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    sidecar = Path(f"{archive}.sha256")
    retained, missing = (archive, sidecar) if occupied == "archive" else (sidecar, archive)
    retained.write_bytes(b"retained")
    assert _run_archive(staged, archive).returncode != 0
    assert retained.read_bytes() == b"retained"
    assert not missing.exists()


def test_max_archive_failure_publishes_no_partial_new_pair(tmp_path: Path) -> None:
    """Archiver failure leaves the new destination absent."""
    staged = tmp_path / "package"
    staged.mkdir()
    archive = tmp_path / "Sunny.zip"
    assert _run_archive(staged, archive, fail_archiver=True).returncode != 0
    assert not archive.exists()
    assert not Path(f"{archive}.sha256").exists()


def test_max_archive_concurrent_publishers_retain_one_matching_pair(tmp_path: Path) -> None:
    """Competing source trees cannot replace or mix the successful publication."""
    stages = [tmp_path / "first", tmp_path / "second"]
    for stage in stages:
        stage.mkdir()
        (stage / "content.txt").write_text(stage.name, encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(lambda stage: _run_archive(stage, archive), stages))
    assert sum(result.returncode == 0 for result in results) == 1
    winner = stages[next(i for i, result in enumerate(results) if result.returncode == 0)]
    with zipfile.ZipFile(archive) as zipped:
        assert zipped.read("Sunny/content.txt") == winner.name.encode()
    expected = f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n"
    assert Path(f"{archive}.sha256").read_text(encoding="ascii") == expected


def test_max_archive_checksum_collision_rolls_back_only_its_new_zip(tmp_path: Path) -> None:
    """A filesystem collision at the second rename preserves the competing file."""
    staged = tmp_path / "package"
    staged.mkdir()
    (staged / "content.txt").write_text("new", encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    script = Path(__file__).parents[2] / "max-package" / "cmake" / "CreateArchive.cmake"
    driver = tmp_path / "collision.cmake"
    # Inject the competing writer at the filesystem rename boundary; all real archive and
    # publication operations still execute through CMake's original file command.
    driver.write_text(
        "macro(file)\n"
        '  if("${ARGV0}" STREQUAL "RENAME" AND\n'
        '     "${ARGV2}" STREQUAL "${SUNNY_MAX_ARCHIVE}.sha256")\n'
        '    _file(WRITE "${SUNNY_MAX_ARCHIVE}.sha256" "competing checksum")\n'
        "  endif()\n"
        "  _file(${ARGV})\n"
        "endmacro()\n"
        f'include("{script.as_posix()}")\n',
        encoding="utf-8",
    )
    result = subprocess.run(
        [
            "cmake",
            f"-DSUNNY_MAX_STAGED_PACKAGE={staged}",
            f"-DSUNNY_MAX_ARCHIVE={archive}",
            "-P",
            str(driver),
        ],
        capture_output=True,
        text=True,
        timeout=30,
    )
    assert result.returncode != 0
    assert "Cannot publish Sunny Max archive checksum" in result.stderr
    assert not archive.exists()
    assert Path(f"{archive}.sha256").read_text() == "competing checksum"


@pytest.mark.parametrize("nested", ["Sunny.zip", "new/sub/Sunny.zip", "../package/Sunny.zip"])
def test_max_archive_rejects_destination_inside_staged_inputs(tmp_path: Path, nested: str) -> None:
    """An output path cannot make packaging recursively consume or alter its own inputs."""
    staged = tmp_path / "package"
    staged.mkdir()
    content = staged / "content.txt"
    content.write_text("source", encoding="utf-8")
    result = _run_archive(staged, staged / nested)
    assert result.returncode != 0
    assert "must be outside the staged package" in result.stderr
    assert list(staged.iterdir()) == [content]
    assert content.read_text() == "source"


def test_max_archive_rejects_aliased_destination_before_creating_directories(
    tmp_path: Path,
) -> None:
    """An existing directory alias still resolves when output parents do not exist yet."""
    staged = tmp_path / "package"
    staged.mkdir()
    content = staged / "content.txt"
    content.write_text("source", encoding="utf-8")
    alias = tmp_path / "alias"
    try:
        alias.symlink_to(staged, target_is_directory=True)
    except OSError:
        pytest.skip("Directory symlinks are unavailable")
    result = _run_archive(staged, alias / "new" / "Sunny.zip")
    assert result.returncode != 0
    assert "must be outside the staged package" in result.stderr
    assert list(staged.iterdir()) == [content]
    assert content.read_text() == "source"


@pytest.mark.parametrize("suffix", ["", "package"])
def test_max_archive_work_directory_cannot_own_staged_inputs(tmp_path: Path, suffix: str) -> None:
    """Work cleanup must not remove a source tree that overlaps its directory."""
    archive = tmp_path / "Sunny.zip"
    staged = tmp_path / "Sunny.zip.work" / suffix
    staged.mkdir(parents=True)
    content = staged / "content.txt"
    content.write_text("source", encoding="utf-8")
    result = _run_archive(staged, archive)
    assert result.returncode != 0
    assert "must be outside the staged package" in result.stderr
    assert list(staged.iterdir()) == [content]
    assert content.read_text() == "source"
    assert not Path(f"{archive}.lock").exists()


def test_max_archive_lock_alias_cannot_truncate_staged_inputs(tmp_path: Path) -> None:
    """CMake lock acquisition must never follow an alias to source bytes."""
    staged = tmp_path / "package"
    staged.mkdir()
    content = staged / "content.txt"
    content.write_text("source", encoding="utf-8")
    archive = tmp_path / "Sunny.zip"
    lock = Path(f"{archive}.lock")
    try:
        lock.symlink_to(content)
    except OSError:
        pytest.skip("File symlinks are unavailable")
    result = _run_archive(staged, archive)
    assert result.returncode != 0
    assert "lock path must not be a symlink" in result.stderr
    assert content.read_text() == "source"
    assert lock.is_symlink()
    assert not archive.exists()
    assert not Path(f"{archive}.sha256").exists()
