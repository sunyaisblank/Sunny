"""Python scripting facade for the Sunny theory engine.

The engine itself is C++ (sunny_native via pybind11); this package adds
constants, note-name maps, and the TheoryEngine convenience wrapper.
The MCP server is the C++ binary sunny-mcp, not this package.
"""

from sunny.core.engine import TheoryEngine, get_engine

__version__ = "0.4.0"

__all__ = ["TheoryEngine", "__version__", "get_engine"]
