"""Compare the authored native stub with the extension from the current CMake build."""

from __future__ import annotations

import importlib
import importlib.util
import os
import runpy
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    """Run mypy.stubtest without admitting a stale editable-install redirect."""
    build_root = Path(
        os.environ.get("SUNNY_NATIVE_BUILD_DIR", PROJECT_ROOT / ".bin" / "python")
    ).resolve()

    sys.meta_path[:] = [
        finder for finder in sys.meta_path if finder.__class__.__module__ != "_editable_skbc_sunny"
    ]
    sys.path.insert(0, str(build_root))
    importlib.invalidate_caches()
    specification = importlib.util.find_spec("sunny_native")
    if specification is None or specification.origin is None:
        raise SystemExit(f"sunny_native is not importable from {build_root}")
    runtime_path = Path(specification.origin).resolve()
    if runtime_path.parent != build_root:
        raise SystemExit(
            f"stubtest resolved {runtime_path}, expected an extension directly under {build_root}"
        )

    os.environ["MYPYPATH"] = str(PROJECT_ROOT / "stubs")
    sys.argv = [
        "stubtest",
        "--concise",
        "--mypy-config-file",
        str(PROJECT_ROOT / "pyproject.toml"),
        "--allowlist",
        str(PROJECT_ROOT / "tests" / "python" / "stubtest_allowlist.txt"),
        "sunny_native",
    ]
    print(f"stubtest runtime: {runtime_path}")
    runpy.run_module("mypy.stubtest", run_name="__main__")


if __name__ == "__main__":
    main()
