# Sunny Architecture

**Document ID:** SUNNY-ARCH-001
**Version:** 3.0
**Date:** February 2026
**Status:** Active

---

## 1. Overview

Sunny is a music theory computation and Ableton Live integration system designed for AI-assisted composition. The architecture follows aerospace-derived naming conventions from the Sirius codebase.

### 1.1 Design Principles

1. **C++ Core** - All theory computation in C++ for performance and exactness
2. **Thin Python Wrapper** - Python layer is minimal, delegates to C++
3. **Exact Arithmetic** - Integer/rational math for pitch and rhythm (no floating-point)
4. **1:1 Test Coverage** - Every component has a corresponding test file
5. **Fail-Safe Defaults** - Invalid input produces errors, not silent failures

### 1.2 Language Distribution

| Language | Purpose | Lines |
|----------|---------|-------|
| C++ | Core theory, render, infrastructure | ~7,600 |
| Python | MCP server, Remote Script, thin wrapper | ~1,500 |

---

## 2. System Architecture

```
+-----------------------------------------------------------------------+
|                         MCP CLIENT (Claude)                            |
+-----------------------------------------------------------------------+
                                    |
                                    | JSON-RPC (MCP Protocol)
                                    v
+-----------------------------------------------------------------------+
|                    PYTHON MCP SERVER (FastMCP)                         |
|  - Tool registration and routing                                       |
|  - Async/await bridging                                                |
|  - JSON serialization                                                  |
|  - ~500 lines                                                          |
+-----------------------------------------------------------------------+
                                    |
                                    | pybind11
                                    v
+-----------------------------------------------------------------------+
|                    SUNNY_NATIVE (C++ via pybind11)                     |
|  Version: 0.3.0                                                        |
|  - Sunny.Core: Music theory primitives                                 |
|  - Sunny.Render: DSP/modulation                                        |
|  - Sunny.Infrastructure: Application services                          |
+-----------------------------------------------------------------------+
            |                                      |
            | Direct calls                         | TCP/UDP
            v                                      v
+-------------------------------+    +----------------------------------+
|       SUNNY.CORE (C++)        |    |   PYTHON HOST TRANSPORT          |
|  - Pitch: Z/12Z group ops     |    |   HSTP001A - AbletonConnection   |
|  - Scale: 35+ definitions     |    |   - TCP for commands             |
|  - Harmony: Functional, neg   |    |   - UDP for low-latency events   |
|  - VoiceLeading: Nearest-tone |    |   - Auto-reconnection            |
|  - Rhythm: Euclidean          |    |   - Message queuing              |
|  - Tensor: Beat arithmetic    |    +----------------------------------+
+-------------------------------+                  |
                                                   | LOM API
                                                   v
                                    +----------------------------------+
                                    |    ABLETON REMOTE SCRIPT         |
                                    |    (Python, required by Ableton) |
                                    +----------------------------------+
                                                   |
                                                   v
                                    +----------------------------------+
                                    |         ABLETON LIVE             |
                                    +----------------------------------+
```

---

## 3. Component Naming Convention

### 3.1 Format: `[Domain][Category][Sequence][Variant]`

| Position | Example | Meaning |
|----------|---------|---------|
| 1-2 | PT | Domain (Pitch) |
| 3-4 | PC | Category (PitchClass) |
| 5-7 | 001 | Sequence number |
| 8 | A | Variant (A=Primary) |

**Example:** `PTPC001A` = Pitch domain, PitchClass category, first component, primary variant

### 3.2 Domain Codes

| Code | Domain | Description |
|------|--------|-------------|
| PT | Pitch | Pitch class operations (Z/12Z group) |
| SC | Scale | Scale definitions and generation |
| HR | Harmony | Functional analysis, chord generation |
| VL | VoiceLeading | Voice leading algorithms |
| RH | Rhythm | Euclidean rhythm, polyrhythm |
| TN | Tensor | Beat arithmetic, note events |
| RD | Render | DSP, modulation, arpeggiator |
| IN | Infrastructure | Application services, transport |
| BD | Binding | pybind11 Python bindings |
| TS | Test | Test components |
| HS | Host | Python host layer |

---

## 4. Directory Structure

### 4.1 C++ Source

```
src/
+-- Sunny.Core/                     # Music theory primitives
|   +-- Pitch/
|   |   +-- PTPC001A.h/cpp         # pitch_class, transpose, invert
|   |   +-- PTMN001A.h/cpp         # MIDI note utilities
|   |   +-- PTPS001A.h/cpp         # Pitch class set operations
|   +-- Scale/
|   |   +-- SCDF001A.h/cpp         # 35+ scale definitions
|   |   +-- SCGN001A.h/cpp         # Scale generation
|   +-- Harmony/
|   |   +-- HRFN001A.h/cpp         # Functional harmony analysis
|   |   +-- HRNG001A.h/cpp         # Negative harmony
|   |   +-- HRRN001A.h/cpp         # Roman numeral parsing
|   +-- VoiceLeading/
|   |   +-- VLNT001A.h/cpp         # Nearest-tone algorithm
|   +-- Rhythm/
|   |   +-- RHEU001A.h/cpp         # Euclidean rhythm (Bresenham)
|   +-- Tensor/
|       +-- TNTP001A.h             # Core types (PitchClass, MidiNote)
|       +-- TNBT001A.h/cpp         # Beat (rational arithmetic)
|       +-- TNEV001A.h/cpp         # NoteEvent, ChordVoicing
|
+-- Sunny.Render/                   # DSP and modulation
|   +-- Transport/
|   |   +-- RDTR001A.h/cpp         # MIDI transport, scheduling
|   +-- Modulation/
|   |   +-- RDMD001A.h/cpp         # LFO, Envelope, Sample&Hold
|   +-- Arpeggio/
|       +-- RDAP001A.h/cpp         # Arpeggiator patterns
|
+-- Sunny.Infrastructure/           # Application services
|   +-- Application/
|   |   +-- INOR001A.h/cpp         # Orchestrator (undo/redo)
|   +-- Bridge/
|   |   +-- INBR001A.h/cpp         # LOM bridge protocol
|   +-- Transport/
|   |   +-- INTP001A.h/cpp         # Network transport (stub)
|   +-- Session/
|   |   +-- INSM001A.h/cpp         # Session state machine
|   +-- Binding/
|       +-- BDPY001A.cpp           # pybind11 module
|
+-- Sunny.Test/                     # C++ test suite
    +-- Unit/
    |   +-- TSPT001A.cpp           # Tests PTPC001A
    |   +-- TSPS001A.cpp           # Tests PTPS001A
    |   +-- TSRH001A.cpp           # Tests RHEU001A
    |   +-- TSSC001A.cpp           # Tests SCDF001A
    |   +-- TSSG001A.cpp           # Tests SCGN001A
    |   +-- TSHF001A.cpp           # Tests HRFN001A
    |   +-- TSHN001A.cpp           # Tests HRNG001A
    |   +-- TSHR001A.cpp           # Tests HRRN001A
    |   +-- TSVL001A.cpp           # Tests VLNT001A
    |   +-- TSTR001A.cpp           # Tests RDTR001A
    |   +-- TSMD001A.cpp           # Tests RDMD001A
    |   +-- TSAP001A.cpp           # Tests RDAP001A
    +-- Diagnostic/
        +-- TSDG001A.cpp           # Z/12Z group invariants
```

### 4.2 Python Source

```
src/sunny/
+-- core/                           # Thin wrapper to C++ backend
|   +-- __init__.py                # Exports: pitch_class, transpose, etc.
|   +-- engine.py                  # TheoryEngine class
|   +-- errors.py                  # Error codes
|   +-- constants.py               # Constants
|
+-- host/                           # Ableton connection
|   +-- transport.py               # HSTP001A - TCP/UDP transport
|   +-- main.py                    # Entry point
|
+-- application/
|   +-- server/                    # MCP server
|       +-- mcp.py                 # FastMCP instance
|       +-- context.py             # Request context
|       +-- lifespan.py            # Server lifecycle
|       +-- tools/                 # MCP tools
|           +-- theory.py          # Theory tools
|           +-- harmony.py         # Harmony tools
|           +-- clip.py            # Clip manipulation
|           +-- track.py           # Track operations
|           +-- session.py         # Session management
|           +-- ...
|
+-- infrastructure/                 # Python infrastructure
|   +-- audit.py                   # Audit logging
|
+-- theory/                         # Legacy (deprecated)
|   +-- rhythm.py                  # Kept for compatibility
|
+-- security/                       # Input validation
    +-- __init__.py
```

### 4.3 Test Structure

```
tests/
+-- conftest.py                     # Pytest fixtures
+-- test_native_core.py             # sunny_native Core tests
+-- test_native_render.py           # sunny_native Render tests
+-- test_native_infrastructure.py   # sunny_native Infrastructure tests
+-- test_python_wrapper.py          # TheoryEngine wrapper tests
+-- test_transport.py               # Transport tests
+-- test_snapshot.py                # Snapshot manager tests
```

---

## 5. Component Registry

### 5.1 Sunny.Core

| Component | File | Description |
|-----------|------|-------------|
| TNTP001A | Tensor/TNTP001A.h | Core types: PitchClass, MidiNote, ErrorCode |
| TNBT001A | Tensor/TNBT001A.h/cpp | Beat type (rational arithmetic) |
| TNEV001A | Tensor/TNEV001A.h/cpp | NoteEvent, ChordVoicing, ScaleDefinition |
| PTPC001A | Pitch/PTPC001A.h/cpp | pitch_class, transpose, invert, interval_class |
| PTMN001A | Pitch/PTMN001A.h/cpp | MIDI note utilities, note_name |
| PTPS001A | Pitch/PTPS001A.h/cpp | Pitch class set: pcs_transpose, pcs_invert, interval_vector |
| SCDF001A | Scale/SCDF001A.h/cpp | 35+ scale definitions |
| SCGN001A | Scale/SCGN001A.h/cpp | generate_scale_notes, is_note_in_scale |
| HRFN001A | Harmony/HRFN001A.h/cpp | Functional harmony: analyze_chord, degree_to_function |
| HRNG001A | Harmony/HRNG001A.h/cpp | negative_harmony transformation |
| HRRN001A | Harmony/HRRN001A.h/cpp | Roman numeral parsing, chord generation |
| VLNT001A | VoiceLeading/VLNT001A.h/cpp | Nearest-tone voice leading |
| RHEU001A | Rhythm/RHEU001A.h/cpp | Euclidean rhythm (Bresenham algorithm) |

### 5.2 Sunny.Render

| Component | File | Description |
|-----------|------|-------------|
| RDTR001A | Transport/RDTR001A.h/cpp | MIDI transport, play/stop/pause |
| RDMD001A | Modulation/RDMD001A.h/cpp | LFO, ADSR Envelope, Sample&Hold |
| RDAP001A | Arpeggio/RDAP001A.h/cpp | Arpeggiator: up/down/updown patterns |

### 5.3 Sunny.Infrastructure

| Component | File | Description |
|-----------|------|-------------|
| INOR001A | Application/INOR001A.h/cpp | Orchestrator: operations, undo/redo |
| INBR001A | Bridge/INBR001A.h/cpp | Bridge message protocol |
| INTP001A | Transport/INTP001A.h/cpp | Network transport (stub) |
| INSM001A | Session/INSM001A.h/cpp | Session state machine |
| BDPY001A | Binding/BDPY001A.cpp | pybind11 Python bindings |

### 5.4 Python Host

| Component | File | Description |
|-----------|------|-------------|
| HSTP001A | host/transport.py | AbletonConnection, TCP/UDP transport |
| CREN001A | core/engine.py | TheoryEngine wrapper |

---

## 6. Invariants

### 6.1 Pitch Class (Z/12Z Group)

```
pitch_class(midi) in [0, 11]
transpose(pc, 0) == pc                           # Identity
transpose(transpose(pc, a), b) == transpose(pc, a + b)  # Associative
invert(invert(pc, axis), axis) == pc             # Self-inverse
interval_class(i) in [0, 6]
interval_class(i) == interval_class(-i)          # Symmetric
```

### 6.2 Euclidean Rhythm

```
len(result) == steps
sum(result) == pulses
# Pattern maximizes minimum inter-onset interval
```

### 6.3 Voice Leading

```
len(result) == len(source)
{p % 12 for p in result} == target_pitch_classes
# Total motion is minimized
```

### 6.4 Beat Arithmetic

```
Beat uses exact rational arithmetic (no floating-point)
gcd(a, b) == gcd(b, a)                          # Commutative
lcm(a, 0) == 0
```

---

## 7. Build System

### 7.1 C++ Build (CMake)

```bash
# Prerequisites
# - CMake 3.20+
# - C++23 compiler (GCC 13+, Clang 16+, MSVC 2022)
# - Python 3.11+ with dev headers

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Run tests
ctest --output-on-failure

# Current status: 87% tests pass (79/91)
```

### 7.2 Python Setup

```bash
# Install in development mode
pip install -e .

# Import sunny_native (requires C++ build)
python -c "import sunny_native; print(sunny_native.__version__)"
# Output: 0.3.0
```

### 7.3 CMake Structure

```
CMakeLists.txt                      # Top-level
+-- src/Sunny.Core/CMakeLists.txt   # Core library
+-- src/Sunny.Render/CMakeLists.txt # Render library
+-- src/Sunny.Infrastructure/CMakeLists.txt  # Infrastructure
+-- src/Sunny.Test/CMakeLists.txt   # Tests
```

---

## 8. Transport Protocol

### 8.1 Python Transport (HSTP001A)

The Python transport handles communication with Ableton Live:

```python
from sunny.host.transport import AbletonConnection, TransportConfig

# Configuration (can load from environment)
config = TransportConfig(
    host="127.0.0.1",
    tcp_port=9001,
    udp_port=9002,
    timeout_seconds=5.0,
    retry_count=3,
)

# Connect
conn = AbletonConnection(config)
await conn.connect()

# Send command
result = await conn.send_command("get_tempo", {"track": 0})

# Low-latency event via UDP
await conn.send_udp_event("note_on", {"pitch": 60, "velocity": 100})
```

### 8.2 Features

- **Auto-reconnection**: Reconnects automatically on connection loss
- **Message queuing**: Queues messages during reconnection
- **Heartbeat**: Monitors connection health
- **State listeners**: Notifies on connection state changes

### 8.3 Connection States

```
DISCONNECTED -> CONNECTING -> CONNECTED
                    |              |
                    v              v
                 ERROR       RECONNECTING
                               |
                               v
                           CONNECTED
```

---

## 9. MCP Integration

### 9.1 Server Architecture

```python
# sunny/application/server/mcp.py
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Sunny")

# Tools are registered via decorators in tools/*.py
```

### 9.2 Tool Flow

```
MCP Client -> FastMCP -> Tool Function -> TheoryEngine -> sunny_native
                                              |
                                              v (fallback)
                                         Pure Python
```

### 9.3 TheoryEngine Pattern

```python
# sunny/core/engine.py
class TheoryEngine:
    def __init__(self):
        self._native = sunny_native if NATIVE_AVAILABLE else None

    def get_scale_notes(self, root, mode, octave):
        if self._native:
            return self._native.generate_scale_notes(root_pc, intervals, octave)
        # Fallback to pure Python
        return [base_midi + i for i in intervals]
```

---

## 10. Testing Strategy

### 10.1 Test Naming

Every C++ component `XXAA###V` has a test `TSXX###V.cpp`:

| Source | Test |
|--------|------|
| PTPC001A | TSPT001A |
| SCDF001A | TSSC001A |
| RHEU001A | TSRH001A |

### 10.2 Python Integration Tests

```
tests/
+-- test_native_core.py       # Tests sunny_native.pitch_class, etc.
+-- test_native_render.py     # Tests sunny_native.Lfo, Envelope, etc.
+-- test_native_infrastructure.py  # Tests SessionStateMachine, Orchestrator
+-- test_python_wrapper.py    # Tests TheoryEngine wrapper
+-- test_transport.py         # Tests AbletonConnection
```

### 10.3 Running Tests

```bash
# C++ tests
cd build && ctest --output-on-failure

# Python tests (requires pytest)
PYTHONPATH=src pytest tests/ -v
```

---

## 11. Migration Status

### Completed

- [x] C++ Core implementation (all subsystems)
- [x] C++ Render implementation (LFO, Envelope, Arpeggiator, Transport)
- [x] C++ Infrastructure (Orchestrator, Session, Bridge)
- [x] pybind11 bindings (BDPY001A v0.3.0)
- [x] 1:1 unit tests (13 test files)
- [x] Python thin wrapper (TheoryEngine)
- [x] Python transport with auto-reconnection
- [x] Integration tests
- [x] Lazy imports (no MCP dependency for core)

### Statistics

- **C++ LOC:** ~7,600
- **Python LOC:** ~1,500
- **Test pass rate:** 87% (79/91 C++ tests)
- **C++ Standard:** C++23 (for std::expected)

---

## 12. Future Work

1. **CI/CD Pipeline** - GitHub Actions for build/test
2. **Max/RNBO Integration** - Sample-accurate DSP externals
3. **Performance Benchmarks** - Native vs Python comparison
4. **Full Remote Script** - Complete LOM bridge implementation
