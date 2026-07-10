"""Pytest configuration and fixtures.

Provides common fixtures for integration testing.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

# Project root: src/Sunny.Test/Python/ → src/Sunny.Test/ → src/ → root
PROJECT_ROOT = Path(__file__).parent.parent.parent.parent

# Add build directory to path for sunny_native
BUILD_DIR = PROJECT_ROOT / ".bin" / "src" / "Sunny.Infrastructure"
if BUILD_DIR.exists():
    sys.path.insert(0, str(BUILD_DIR))

# Add src to path for Python modules
SRC_DIR = PROJECT_ROOT / "src"
sys.path.insert(0, str(SRC_DIR))


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
