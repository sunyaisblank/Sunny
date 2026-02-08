"""Pytest configuration and fixtures.

Provides common fixtures for integration testing.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

# Project root: src/Sunny.Test/Python/ → src/Sunny.Test/ → src/ → root
PROJECT_ROOT = Path(__file__).parent.parent.parent.parent

# Add build directory to path for sunny_native
BUILD_DIR = PROJECT_ROOT / "build" / "src" / "Sunny.Infrastructure"
if BUILD_DIR.exists():
    sys.path.insert(0, str(BUILD_DIR))

# Add src to path for Python modules
SRC_DIR = PROJECT_ROOT / "src"
sys.path.insert(0, str(SRC_DIR))


@pytest.fixture
def native_available() -> bool:
    """Check if native backend is available."""
    try:
        import sunny_native
        return True
    except ImportError:
        return False


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
    """Get a TheoryEngine instance."""
    from sunny.core.engine import TheoryEngine
    return TheoryEngine()


@pytest.fixture
def snapshot_dir(tmp_path):
    """Create temporary snapshot directory."""
    snapshot_path = tmp_path / "snapshots"
    snapshot_path.mkdir()
    return snapshot_path
