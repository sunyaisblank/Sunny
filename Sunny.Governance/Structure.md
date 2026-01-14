# Sunny - Structural Compliance Standard

**Document ID:** SUNNY-STD-STRUC-001  
**Version:** 1.0  
**Date:** December 2025  
**Status:** Active

---

## 1. Scope

This standard establishes naming conventions and structural organization for the Sunny project. It applies to all source code, tests, and documentation.

---

## 2. Directory Structure

| Directory | Domain | Description |
|-----------|--------|-------------|
| `Sunny.Server` | MCP | FastMCP server, tools, transport layer |
| `Sunny.Theory` | Music | Theory engine, scales, harmony, rhythm |
| `Sunny.RemoteScript` | Ableton | Remote Script, Max for Live devices |
| `Sunny.Test` | Testing | Unit, integration, benchmark tests |
| `Sunny.Governance` | Documentation | Standards, roadmap, compliance docs |
| `.agent/workflows` | Automation | Agent workflow definitions |

---

## 3. Component Naming Convention

### 3.1 Format

```
[Domain][Category][Sequence][Variant]
```

**Example:** `SVSV001A`
- **Domain**: `SV` (Server)
- **Category**: `SV` (Server)
- **Sequence**: `001` (First component)
- **Variant**: `A` (Primary implementation)

### 3.2 Domain Codes

| Code | Domain | Description |
|------|--------|-------------|
| `SV` | Server | MCP server, tools, transport |
| `TH` | Theory | Music theory, scales, harmony |
| `RS` | RemoteScript | Ableton Remote Script |
| `TS` | Test | Test cases |
| `GV` | Governance | Documentation |

### 3.3 Category Codes

#### 3.3.1 Server (SV)

| Code | Category | Description |
|------|----------|-------------|
| `SV` | Server | Main server entry point |
| `TL` | Tool | MCP tool implementations |
| `TR` | Transport | TCP/UDP transport layer |
| `SN` | Snapshot | Project snapshot system |

#### 3.3.2 Theory (TH)

| Code | Category | Description |
|------|----------|-------------|
| `EN` | Engine | Core theory engine |
| `SC` | Scale | Scale and mode handling |
| `CH` | Chord | Chord and harmony |
| `RH` | Rhythm | Rhythm and timing |

#### 3.3.3 Remote Script (RS)

| Code | Category | Description |
|------|----------|-------------|
| `SR` | Script | Main Remote Script |
| `M4` | MaxForLive | Max for Live devices |
| `HD` | Handler | Command handlers |

#### 3.3.4 Test (TS)

| Code | Category | Description |
|------|----------|-------------|
| `UN` | Unit | Component-level unit tests |
| `IN` | Integration | End-to-end integration tests |
| `BM` | Benchmark | Performance benchmarks |

### 3.4 Variant Codes

| Code | Meaning | Usage |
|------|---------|-------|
| `A` | Primary | Production-ready |
| `B` | Alternative | Alternative algorithm |
| `X` | Experimental | Under development |
| `D` | Deprecated | Superseded |

---

## 4. File Naming

### Source Files
- Python modules: `module_name.py` (snake_case)
- Main entry: `server.py`, `__init__.py`

### Tests
- Unit: `test_<module>.py`
- Integration: `test_integration.py`
- Benchmarks: `bench_<feature>.py`

### Documentation
- Standards: `<Name>.md` (PascalCase)
- Workflows: `<name>.md` (kebab-case)

---

## 5. Python Style Guide

### Imports
```python
# Standard library
from __future__ import annotations
import json
import socket

# Third-party
from mcp.server.fastmcp import FastMCP
from music21 import scale, chord

# Local
from sunny.theory import TheoryEngine
```

### Type Hints
All public APIs must include type hints:
```python
def generate_progression(
    root: str,
    scale_name: str,
    numerals: list[str],
    octave: int = 4
) -> list[dict[str, list[int]]]:
    ...
```

### Docstrings
Google-style docstrings for all public functions:
```python
def get_scale_notes(root: str, mode: str) -> list[int]:
    """Get MIDI note numbers for scale degrees.
    
    Args:
        root: Root note (e.g., "C", "F#")
        mode: Scale mode (e.g., "ionian", "dorian")
    
    Returns:
        List of MIDI note numbers for scale degrees 1-7
    
    Raises:
        ValueError: If mode is not recognized
    """
```

---

## 6. MCP Tool Guidelines

### Tool Registration
```python
@mcp.tool()
def tool_name(
    ctx: Context,
    required_param: str,
    optional_param: int = 0
) -> dict:
    """One-line description.
    
    Detailed description of what this tool does.
    
    Args:
        required_param: Description
        optional_param: Description (default: 0)
    
    Returns:
        Dictionary with operation result
    """
```

### Naming Conventions
- Use `snake_case` for tool names
- Prefix with category: `create_`, `get_`, `set_`, `delete_`
- Examples: `create_midi_track`, `get_session_info`, `set_tempo`

### Error Handling
```python
@mcp.tool()
def risky_operation(ctx: Context) -> str:
    try:
        result = ableton.send_command("risky_op")
        return json.dumps(result)
    except ConnectionError as e:
        logger.error(f"Connection lost: {e}")
        return f"Error: Connection to Ableton lost"
    except Exception as e:
        logger.exception("Unexpected error")
        return f"Error: {str(e)}"
```

---

## 7. Compliance

All changes to the codebase must adhere to this standard. Non-compliant components will be flagged during code review and CI checks.
