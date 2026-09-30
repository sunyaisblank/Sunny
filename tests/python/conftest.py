"""Shared pytest configuration and fixtures.

Provides common fixtures for integration testing.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

# Project root: tests/python/ → tests/ → root
PROJECT_ROOT = Path(__file__).resolve().parents[2]

# An editable scikit-build-core install registers a meta-path finder ahead of
# Python's normal path finder. Without removing this test-only redirect, it can
# load an older editable build even when `.bin/python` is first on PYTHONPATH.
# The repository gate must exercise the extension produced by the current CMake
# build, not whichever editable wheel happened to be installed previously.
sys.meta_path[:] = [
    finder for finder in sys.meta_path if finder.__class__.__module__ != "_editable_skbc_sunny"
]

# Add build directory to path for sunny_native
BUILD_DIR = PROJECT_ROOT / ".bin" / "python"
if BUILD_DIR.exists():
    sys.path.insert(0, str(BUILD_DIR))

# Add both Python packages to the import path.
sys.path.insert(0, str(PROJECT_ROOT / "python"))
sys.path.insert(0, str(PROJECT_ROOT / "remote_script"))


@pytest.fixture
def native_available() -> bool:
    """Check if native backend is available."""
    from sunny.core import NATIVE_AVAILABLE

    return NATIVE_AVAILABLE


@pytest.fixture
def sunny_native_module():
    """Get sunny_native module, skip if unavailable."""
    try:
        import sunny_native

        return sunny_native
    except ImportError:
        pytest.skip("sunny_native not built")


@pytest.fixture
def theory_engine():
    """Get a TheoryEngine instance, skip if the native backend is absent."""
    from sunny.core import NATIVE_AVAILABLE

    if not NATIVE_AVAILABLE:
        pytest.skip("sunny_native not built")
    from sunny.core.engine import TheoryEngine

    return TheoryEngine()
