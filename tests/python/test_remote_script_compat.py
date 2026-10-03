"""The Remote Script must stay loadable by the oldest Python that Live embeds."""

from __future__ import annotations

import ast
from pathlib import Path

import pytest

# Live 11 embeds Python 3.7; Live 12 embeds 3.11. The script is loaded by
# Live's own interpreter, not by the project's development interpreter.
_OLDEST_LIVE_PYTHON = (3, 7)
_SCRIPT_ROOT = Path(__file__).parents[2] / "remote_script" / "Sunny"


@pytest.mark.parametrize("source", sorted(_SCRIPT_ROOT.glob("*.py")), ids=lambda path: path.name)
def test_remote_script_parses_under_the_oldest_live_python(source: Path) -> None:
    """Syntax newer than Live 11's interpreter would fail at control-surface load."""
    ast.parse(source.read_text(encoding="utf-8"), feature_version=_OLDEST_LIVE_PYTHON)


@pytest.mark.parametrize("source", sorted(_SCRIPT_ROOT.glob("*.py")), ids=lambda path: path.name)
def test_remote_script_calls_zip_without_the_python_310_strict_keyword(source: Path) -> None:
    """zip(strict=...) parses under 3.7 but raises TypeError there when it runs."""
    tree = ast.parse(source.read_text(encoding="utf-8"))
    offending = [
        node.lineno
        for node in ast.walk(tree)
        if isinstance(node, ast.Call)
        and isinstance(node.func, ast.Name)
        and node.func.id == "zip"
        and any(keyword.arg == "strict" for keyword in node.keywords)
    ]
    assert offending == []
