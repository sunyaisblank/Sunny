"""Verify the exported adapter bytes and native expected source identity offline."""

from __future__ import annotations

import hashlib
import importlib.util
import shutil
import subprocess
from pathlib import Path
from types import ModuleType

import pytest

_PROJECT_ROOT = Path(__file__).resolve().parents[2]
_SCRIPT_ROOT = _PROJECT_ROOT / "remote_script" / "Sunny"


@pytest.fixture
def identity_module() -> ModuleType:
    """Load the standalone standard-library identity module without a Live host."""
    spec = importlib.util.spec_from_file_location(
        "sunny_bundle_identity_test", _SCRIPT_ROOT / "build_identity.py"
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _independent_identity(root: Path) -> str:
    # Deliberately use bytes and the literal contract rather than the producer's helper.
    chunks = [b"sunny-remote-script-v1\n"]
    for relative in sorted(
        path.relative_to(root).as_posix() for path in root.rglob("*.py") if path.is_file()
    ):
        chunks.extend(
            [
                relative.encode("ascii"),
                b"\n",
                hashlib.sha256((root / relative).read_bytes()).hexdigest().encode("ascii"),
                b"\n",
            ]
        )
    return hashlib.sha256(b"".join(chunks)).hexdigest()


def _copy_sources(tmp_path: Path) -> Path:
    source = tmp_path / "source" / "Sunny"
    shutil.copytree(_SCRIPT_ROOT, source, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    return source


def test_source_identity_closes_over_actual_module_bytes(identity_module: ModuleType) -> None:
    """Check the import-time value against an independently built canonical stream."""
    assert identity_module.BRIDGE_SOURCE_SHA256 == _independent_identity(_SCRIPT_ROOT)
    assert len(identity_module.BRIDGE_SOURCE_SHA256) == 64


def test_modified_added_and_missing_sources_change_identity(
    identity_module: ModuleType, tmp_path: Path
) -> None:
    """Detect edited bytes and new modules; diagnose a missing essential module."""
    source = _copy_sources(tmp_path)
    original = identity_module.compute_source_identity(source)
    (source / "managed.py").write_bytes((source / "managed.py").read_bytes() + b"\n# modified\n")
    edited = identity_module.compute_source_identity(source)
    assert edited != original
    assert edited == _independent_identity(source)
    (source / "late_module.py").write_bytes(b"VALUE = 12\n")
    added = identity_module.compute_source_identity(source)
    assert added != edited
    assert added == _independent_identity(source)
    (source / "handler.py").unlink()
    with pytest.raises(RuntimeError, match="missing.*handler.py.*matching server image"):
        identity_module.compute_source_identity(source)


def test_identity_uses_raw_bytes_and_ignores_bytecode(
    identity_module: ModuleType, tmp_path: Path
) -> None:
    """Keep source encoding/newlines significant while excluding interpreter caches."""
    source = _copy_sources(tmp_path)
    baseline = identity_module.compute_source_identity(source)
    cache = source / "__pycache__"
    cache.mkdir()
    (cache / "handler.cpython-311.pyc").write_bytes(b"ignored bytecode")
    (source / "directory.py").mkdir()
    assert identity_module.compute_source_identity(source) == baseline
    (source / "late_module.py").write_bytes(b"TEXT = '\xc3\xa9'\r\n")
    assert identity_module.compute_source_identity(source) == _independent_identity(source)
    windows_lines = identity_module.compute_source_identity(source)
    (source / "late_module.py").write_bytes(b"TEXT = '\xc3\xa9'\n")
    assert identity_module.compute_source_identity(source) != windows_lines


def test_cmake_exports_exact_bundle_and_reconfigures_for_late_modules(
    identity_module: ModuleType, tmp_path: Path
) -> None:
    """Run only the standalone packaging configure graph, without native compilation."""
    cmake = shutil.which("cmake")
    if cmake is None:
        pytest.skip("CMake is required to qualify source-bundle generation")
    source = _copy_sources(tmp_path)
    project = tmp_path / "fixture"
    project.mkdir()
    build = tmp_path / "build"
    module = (_PROJECT_ROOT / "cmake" / "BridgeBundle.cmake").as_posix()
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\nproject(BundleFixture NONE)\n"
        f'include("{module}")\n'
        f'sunny_prepare_bridge_bundle("{source.as_posix()}" '
        '"${CMAKE_BINARY_DIR}/bundle/Sunny" identity)\n'
        'file(WRITE "${CMAKE_BINARY_DIR}/expected.sha256" "${identity}\\n")\n'
        'add_custom_target(bundle_fixture ALL COMMAND "${CMAKE_COMMAND}" -E true)\n',
        encoding="utf-8",
    )
    configure = subprocess.run(
        [cmake, "-S", str(project), "-B", str(build)],
        text=True,
        capture_output=True,
        check=False,
    )
    assert configure.returncode == 0, configure.stdout + configure.stderr
    bundle = build / "bundle" / "Sunny"
    expected = (build / "expected.sha256").read_text().strip()
    assert expected == identity_module.compute_source_identity(bundle)
    assert expected == _independent_identity(source)
    for name in [p.relative_to(source) for p in source.rglob("*.py") if p.is_file()] + [
        Path("bridge_contract.json")
    ]:
        assert (bundle / name).read_bytes() == (source / name).read_bytes()
    assert (bundle / "source.sha256").read_text().strip() == expected
    assert not list(bundle.rglob("*.pyc"))

    (source / "late_module.py").write_bytes(b"ADDED = True\n")
    refresh = subprocess.run(
        [cmake, "--build", str(build)], text=True, capture_output=True, check=False
    )
    assert refresh.returncode == 0, refresh.stdout + refresh.stderr
    added = (build / "expected.sha256").read_text().strip()
    assert added != expected
    assert added == _independent_identity(source)
    assert (bundle / "late_module.py").read_bytes() == b"ADDED = True\n"
    (source / "late_module.py").unlink()
    refresh = subprocess.run(
        [cmake, "--build", str(build)], text=True, capture_output=True, check=False
    )
    assert refresh.returncode == 0, refresh.stdout + refresh.stderr
    assert not (bundle / "late_module.py").exists()
    assert (build / "expected.sha256").read_text().strip() == expected


def test_cmake_rejects_an_incomplete_bundle(tmp_path: Path) -> None:
    """Configuration must name the missing source rather than stage partial delivery."""
    cmake = shutil.which("cmake")
    if cmake is None:
        pytest.skip("CMake is required to qualify source-bundle generation")
    source = _copy_sources(tmp_path)
    (source / "managed.py").unlink()
    project = tmp_path / "fixture"
    project.mkdir()
    module = (_PROJECT_ROOT / "cmake" / "BridgeBundle.cmake").as_posix()
    (project / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\nproject(BundleFixture NONE)\n"
        f'include("{module}")\n'
        f'sunny_prepare_bridge_bundle("{source.as_posix()}" '
        '"${CMAKE_BINARY_DIR}/bundle/Sunny" identity)\n',
        encoding="utf-8",
    )
    result = subprocess.run(
        [cmake, "-S", str(project), "-B", str(tmp_path / "build")],
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode != 0
    assert "managed.py" in result.stderr and "incomplete" in result.stderr
