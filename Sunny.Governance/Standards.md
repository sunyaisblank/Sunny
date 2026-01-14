# Sunny - Technical Standards

**Document ID:** SUNNY-STD-TECH-001  
**Version:** 1.1  
**Date:** December 2025  
**Status:** Active

---

## 1. Performance Requirements

### 1.1 Latency Targets

| Operation | Target | Maximum | Measurement |
|-----------|--------|---------|-------------|
| TCP Command Round-Trip | <50ms | 200ms | Benchmark test |
| UDP/OSC Parameter Update | <5ms | 20ms | Benchmark test |
| Theory Engine Computation | <10ms | 50ms | Unit test timing |
| Snapshot Creation | <500ms | 2000ms | Integration test |
| Browser Scan (full) | <5s | 30s | Integration test |
| Plugin Preset Discovery | <10s | 30s | Integration test |

### 1.2 Resource Limits

| Resource | Limit | Rationale |
|----------|-------|-----------|
| Memory per request | 50MB | Prevent OOM |
| Concurrent connections | 10 | Socket pool size |
| Snapshot storage | 1GB | Disk space management |
| Log file rotation | 100MB | Prevent disk fill |
| Browser scan items | 10,000 | Prevent infinite loops |
| Automation breakpoints | 1,000 | Performance |

---

## 2. Component Naming Convention

### 2.1 Format

All components use a hierarchical significant-digit coding system:

`[Domain][Category][Sequence][Variant]`

**Example:** `THSC001A`
- **Domain**: `TH` (Theory)
- **Category**: `SC` (Scale)
- **Sequence**: `001` (First component)
- **Variant**: `A` (Primary implementation)

### 2.2 Domain Codes

| Code | Domain | Description |
|------|--------|-------------|
| `TH` | Theory | Music theory computations |
| `RS` | RemoteScript | Ableton Live API interactions |
| `SR` | Server | MCP server and transport |
| `AU` | Automation | Clip automation operations |
| `AR` | Arrangement | Arrangement view operations |
| `BR` | Browser | Browser navigation and plugin loading |
| `TS` | Test | Unit and integration tests |

### 2.3 Variant Codes

| Code | Meaning | Usage |
|------|---------|-------|
| `A` | Primary | Production-ready, validated implementation |
| `B` | Alternative | Alternative algorithm or approach |
| `X` | Experimental | Under development, not validated |
| `D` | Deprecated | Superseded, retained for compatibility |

---

## 3. Safety Requirements

### 3.1 Destructive Operation Protocol

**Any operation that modifies or deletes data MUST:**

1. Create an automatic Project Snapshot before execution
2. Log the operation with timestamp and parameters
3. Return confirmation including snapshot ID

**Destructive operations include:**
- `delete_track`
- `delete_clip`
- `clear_arrangement`
- `reset_device`
- Any operation with `delete`, `clear`, `reset` in name

### 3.2 Snapshot System

```python
class SnapshotPolicy:
    """Snapshot creation policy."""
    
    # Snapshot before these operations
    DESTRUCTIVE_OPS = [
        "delete_track",
        "delete_clip",
        "clear_arrangement",
        "reset_device",
    ]
    
    # Retention policy
    MAX_SNAPSHOTS = 50
    MAX_AGE_HOURS = 24
```

### 3.3 Error Recovery

| Error Type | Recovery Action |
|------------|-----------------|
| Connection Lost | Automatic reconnect with exponential backoff |
| Command Timeout | Retry once, then return error |
| Invalid Parameters | Return validation error, no retry |
| Ableton Crash | Wait for Ableton, attempt reconnect |

---

## 4. Communication Protocol

### 4.1 TCP Protocol (Reliable)

**Port:** 9876 (configurable via environment)

**Message Format:**
```json
{
  "type": "command_name",
  "params": {
    "key": "value"
  },
  "id": "uuid"
}
```

**Response Format:**
```json
{
  "status": "success",
  "result": {...},
  "id": "uuid"
}
```

**Error Response:**
```json
{
  "status": "error",
  "message": "Description",
  "code": 1001,
  "id": "uuid"
}
```

### 4.2 UDP/OSC Protocol (Fast)

**Port:** 9877 (configurable via environment)

**OSC Address Space:**
```
/track/<index>/volume <float 0.0-1.0>
/track/<index>/pan <float -1.0-1.0>
/track/<index>/device/<index>/param/<index> <float 0.0-1.0>
/transport/play
/transport/stop
```

### 4.3 Error Codes

| Code | Category | Description |
|------|----------|-------------|
| 1000-1099 | Connection | Transport layer errors |
| 1100-1199 | Validation | Invalid parameters |
| 1200-1299 | Ableton | Live API errors |
| 1300-1399 | Theory | Music theory errors |
| 1400-1499 | Internal | Server internal errors |

---

## 5. Music Theory Standards

### 5.1 Note Representation

| Format | Example | Usage |
|--------|---------|-------|
| MIDI number | 60 | Internal, clips |
| Scientific pitch | C4 | Display, logs |
| Scale degree | 1, b3, #4 | Theory engine |

### 5.2 Chord Representation

```python
Chord = {
    "root": "C",           # Root note name
    "quality": "minor7",   # Chord quality
    "notes": [60, 63, 67, 70],  # MIDI notes
    "inversion": 0,        # 0 = root position
    "voicing": "close",    # close/open/drop2/drop3
}
```

### 5.3 Progression Representation

```python
Progression = [
    {"numeral": "ii", "chord": {...}},
    {"numeral": "V", "chord": {...}},
    {"numeral": "I", "chord": {...}},
]
```

### 5.4 Scale/Mode Registry

| Mode | Intervals | Example (C) |
|------|-----------|-------------|
| Ionian (Major) | W-W-H-W-W-W-H | C D E F G A B |
| Dorian | W-H-W-W-W-H-W | C D Eb F G A Bb |
| Phrygian | H-W-W-W-H-W-W | C Db Eb F G Ab Bb |
| Lydian | W-W-W-H-W-W-H | C D E F# G A B |
| Mixolydian | W-W-H-W-W-H-W | C D E F G A Bb |
| Aeolian (Minor) | W-H-W-W-H-W-W | C D Eb F G Ab Bb |
| Locrian | H-W-W-H-W-W-W | C Db Eb F Gb Ab Bb |

---

## 6. Testing Requirements

### 6.1 Coverage Requirements

| Component | Minimum Coverage |
|-----------|------------------|
| Theory Engine | 100% |
| Transport Layer | 90% |
| MCP Tools | 100% (one test per tool) |
| Remote Script | 80% (mock Ableton) |

### 6.2 Test Categories

| Category | Gate Type | CI Blocking |
|----------|-----------|-------------|
| Unit Tests | Mandatory | Yes |
| Integration Tests | Soft | No (requires Ableton) |
| Benchmark Tests | Soft | No |
| Snapshot Tests | Mandatory | Yes |

### 6.3 Test Naming

```python
def test_<operation>_<scenario>():
    """Test <operation> when <scenario>."""

# Examples:
def test_generate_progression_major_ii_V_I():
def test_get_scale_notes_invalid_mode_raises():
def test_tcp_send_timeout_retries_once():
```

---

## 7. Logging Standards

### 7.1 Log Levels

| Level | Usage |
|-------|-------|
| DEBUG | Detailed diagnostic info |
| INFO | Normal operations |
| WARNING | Unexpected but handled |
| ERROR | Failed operations |
| CRITICAL | System failure |

### 7.2 Log Format

```python
logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    level=logging.INFO
)
```

### 7.3 Sensitive Data

**Never log:**
- Full project file paths
- User credentials
- Plugin license info

**Always log:**
- Operation name and parameters
- Timing information
- Error details

---

## 8. Configuration

### 8.1 Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SUNNY_TCP_PORT` | 9876 | TCP server port |
| `SUNNY_UDP_PORT` | 9877 | UDP/OSC port |
| `SUNNY_LOG_LEVEL` | INFO | Logging level |
| `SUNNY_SNAPSHOT_DIR` | ~/.sunny/snapshots | Snapshot storage |
| `SUNNY_MAX_SNAPSHOTS` | 50 | Max retained snapshots |

### 8.2 Configuration File

```toml
# sunny.toml
[server]
tcp_port = 9876
udp_port = 9877

[theory]
default_scale = "major"
default_root = "C"

[safety]
auto_snapshot = true
max_snapshots = 50
```
