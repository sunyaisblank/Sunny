# Sunny Operational Governance

**Document ID:** SUNNY-OPS-001  
**Version:** 1.0  
**Date:** December 2025  
**Status:** Active  

---

## 1. Scope

This document establishes operational procedures, deterministic execution standards, mathematical bounds, and validation constraints for the Sunny MCP system. All agentic operations must be mathematically bounded with clear assumptions, constraints, and restrictions.

---

## 2. Design Principles

### 2.1 Deterministic Execution

All operations in Sunny follow deterministic execution principles:

| Principle | Description | Implementation |
|-----------|-------------|----------------|
| **Bounded Parameters** | Every parameter has explicit min/max/default | Validation on entry |
| **Idempotent Operations** | Same input → same output | State-free transforms |
| **Traceable State** | All mutations logged with reversibility | Snapshot system |
| **Fail-Safe Defaults** | Invalid input → safe fallback | Default value hierarchy |

### 2.2 Mathematical Boundedness

Every operation must define:

```
Operation(params) → Result where:
  - INPUT:  { param: Type ∈ [min, max] | default }
  - OUTPUT: { field: Type ∈ [min, max] }
  - ERRORS: { error_code: condition }
  - INVARIANTS: { mathematical_constraint }
```

---

## 3. Component Naming Convention

### 3.1 Format

Sunny uses a hierarchical component coding system:

`[Domain][Category][Sequence][Variant]`

**Example:** `THSC001A`
- **Domain**: `TH` (Theory)
- **Category**: `SC` (Scale)
- **Sequence**: `001` (First component)
- **Variant**: `A` (Primary implementation)

### 3.2 Domain Codes

| Code | Domain | Description |
|------|--------|-------------|
| `TH` | Theory | Music theory computations |
| `RS` | RemoteScript | Ableton Live API interactions |
| `SR` | Server | MCP server and transport |
| `AU` | Automation | Clip automation operations |
| `AR` | Arrangement | Arrangement view operations |
| `BR` | Browser | Browser navigation and plugin loading |
| `TS` | Test | Unit and integration tests |

### 3.3 Category Codes

#### 3.3.1 Theory (TH)
| Code | Category | Description |
|------|----------|-------------|
| `SC` | Scale | Scale/mode computations |
| `CH` | Chord | Chord generation and analysis |
| `PG` | Progression | Chord progression logic |
| `VL` | VoiceLeading | Voice leading algorithms |
| `ML` | Melody | Melody generation |
| `OR` | Orchestration | Instrument and voicing |
| `CA` | Cadence | Cadence patterns |

#### 3.3.2 RemoteScript (RS)
| Code | Category | Description |
|------|----------|-------------|
| `TK` | Track | Track manipulation |
| `CL` | Clip | Clip operations |
| `DV` | Device | Device/plugin control |
| `TR` | Transport | Playback control |
| `SN` | Snapshot | Project snapshots |

#### 3.3.3 Browser (BR)
| Code | Category | Description |
|------|----------|-------------|
| `SC` | Scan | Browser scanning |
| `LD` | Load | Device loading |
| `DS` | Discovery | Plugin discovery |

### 3.4 Variant Codes

| Code | Meaning |
|------|---------|
| `A` | Primary, production-ready |
| `B` | Alternative algorithm |
| `X` | Experimental |
| `D` | Deprecated |

---

## 4. Bounded Operation Definitions

### 4.1 Theory Operations

#### THSC001A - get_scale_notes

```yaml
component: THSC001A
name: get_scale_notes
description: Generate MIDI note numbers for a scale

input:
  root:
    type: string
    domain: [C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb, B]
    default: C
  mode:
    type: string
    domain: [ionian, dorian, phrygian, lydian, mixolydian, aeolian, locrian, 
             harmonic_minor, melodic_minor, phrygian_dominant, whole_tone, 
             diminished, augmented, blues, pentatonic_major, pentatonic_minor]
    default: ionian
  octave:
    type: integer
    range: [0, 9]
    default: 4

output:
  notes:
    type: array[integer]
    length: 7
    element_range: [0, 127]  # MIDI note range

invariants:
  - ∀ i ∈ [0,6]: notes[i] < notes[i+1]  # Ascending order
  - notes[0] ≡ root_midi mod 12  # Correct root
  - notes[7] - notes[0] = 12  # Octave span

errors:
  1301: invalid_root
  1302: invalid_mode
  1303: octave_out_of_range
```

#### THCH001A - generate_progression

```yaml
component: THCH001A
name: generate_progression
description: Generate chord progression from Roman numerals

input:
  root:
    type: string
    domain: valid_note_names
    default: C
  scale:
    type: string
    domain: valid_scale_names
  numerals:
    type: array[string]
    max_length: 32
    element_domain: [I, i, II, ii, III, iii, IV, iv, V, v, VI, vi, VII, vii,
                     bII, bIII, bVI, bVII, #IV, V/V, V/ii, etc.]
  octave:
    type: integer
    range: [2, 6]
    default: 4

output:
  chords:
    type: array[Chord]
    max_length: 32
  Chord:
    numeral: string
    root: string
    quality: string ∈ [major, minor, diminished, augmented, dominant7, ...]
    notes: array[integer] where ∀ n: n ∈ [24, 108]

invariants:
  - len(output.chords) = len(input.numerals)
  - ∀ chord: 3 ≤ len(chord.notes) ≤ 7  # Triads to 13ths

errors:
  1304: invalid_numeral
  1305: progression_too_long
```

#### THVL001A - generate_voiced_progression

```yaml
component: THVL001A
name: generate_voiced_progression
description: Generate progression with smooth voice leading

input:
  [inherits THCH001A.input]

output:
  chords:
    type: array[VoicedChord]
  VoicedChord:
    block_notes: array[integer]  # Root position
    voiced_notes: array[integer]  # Voice-led

invariants:
  # Voice leading constraint: minimize total movement
  - ∀ i ∈ [1, len-1]: 
      Σ |voiced_notes[i][j] - voiced_notes[i-1][k]| < 
      Σ |block_notes[i][j] - block_notes[i-1][k]|
  
  # Maximum voice leap: minor 6th (8 semitones)
  - ∀ voice_transition: |Δpitch| ≤ 8

algorithm:
  type: nearest_tone
  complexity: O(n * v²)  # n chords, v voices
```

### 4.2 Automation Operations

#### AUSP001A - set_clip_automation

```yaml
component: AUSP001A
name: set_clip_automation
description: Set automation envelope breakpoints

input:
  track_index:
    type: integer
    range: [0, max_tracks - 1]
  clip_slot:
    type: integer
    range: [0, max_slots - 1]
  parameter_path:
    type: string
    format: "devices/{device_idx}/parameters/{param_idx}"
    pattern: ^devices/\d+/parameters/\d+$
  breakpoints:
    type: array[Breakpoint]
    max_length: 1000
  Breakpoint:
    time:
      type: float
      range: [0.0, clip_length]
    value:
      type: float
      range: [0.0, 1.0]  # Normalized

output:
  set: integer  # Count of breakpoints set
  parameter: string

invariants:
  - ∀ i < j: breakpoints[i].time ≤ breakpoints[j].time  # Time-ordered
  - breakpoints[0].time = 0.0  # Start at clip beginning
  - breakpoints[-1].time ≤ clip.length

errors:
  1401: track_not_found
  1402: clip_not_found
  1403: device_not_found
  1404: parameter_not_found
  1405: breakpoints_empty
  1406: invalid_time_order
```

### 4.3 Browser Operations

#### BRDS001A - discover_plugin_presets

```yaml
component: BRDS001A
name: discover_plugin_presets
description: Scan browser for VST/AU/NKS plugin presets

input:
  manufacturer_filter:
    type: string | null
    default: null
    description: Partial match filter for manufacturer name
  max_depth:
    type: integer
    range: [1, 8]
    default: 4
    description: Maximum browser tree traversal depth

output:
  summary:
    total_manufacturers: integer ≥ 0
    total_plugins: integer ≥ 0
    total_presets: integer ≥ 0
    nks_compatible_manufacturers: integer ≥ 0
  manufacturers:
    type: array[Manufacturer]
    sorted_by: preset_count DESC
  Manufacturer:
    name: string
    is_nks_likely: boolean
    plugins: array[PluginInfo]
    presets: array[PresetInfo]
    preset_count: integer

invariants:
  - summary.total_manufacturers = len(manufacturers)
  - ∀ m: m.preset_count = len(m.presets)
  - Σ m.preset_count = summary.total_presets

algorithm:
  type: breadth_first_scan
  complexity: O(n * d)  # n items, d max_depth
  timeout: 30s

nks_manufacturer_patterns:
  - "native instruments", "ni", "kontakt", "massive", "fm8", "reaktor"
  - "spectrasonics", "omnisphere", "keyscape", "trilian"
  - "arturia", "u-he", "output", "spitfire", "orchestral tools"
  - "fabfilter", "izotope", "waves", "softube"
  - "xfer", "serum", "vital", "lennar digital", "sylenth"
```

### 4.4 Arrangement Operations

#### ARPC001A - place_clip_in_arrangement

```yaml
component: ARPC001A
name: place_clip_in_arrangement
description: Place session clip into arrangement view

input:
  track_index:
    type: integer
    range: [0, max_tracks - 1]
  clip_slot:
    type: integer
    range: [0, max_slots - 1]
  bar:
    type: integer
    range: [1, 9999]
    description: 1-indexed bar number
  beat:
    type: float
    range: [1.0, time_signature_numerator + 0.999]
    default: 1.0

output:
  placed: boolean
  bar: integer
  beat: float
  position_beats: float

invariants:
  - position_beats = (bar - 1) * time_sig + (beat - 1)
  - position_beats ≥ 0
  - clip is copied, not moved (session clip persists)

errors:
  1501: track_not_found
  1502: clip_slot_empty
  1503: bar_out_of_range
  1504: api_not_available (Live version < 11)
```

---

## 5. Device Parameter Control Standards

### 5.1 Parameter Mapping

```yaml
component: RSDV001A
name: device_parameter_interface
description: Standard for VST/AU parameter access

parameter_structure:
  index:
    type: integer
    range: [0, device.parameter_count - 1]
  name:
    type: string
    source: device.parameters[index].name
  value:
    type: float
    range: [param.min, param.max]
  normalized_value:
    type: float
    range: [0.0, 1.0]
    formula: (value - min) / (max - min)

common_parameter_indices:
  device_on: 0  # Almost always
  preset/program: 1-3  # Varies by plugin
  macro_1_8: varies  # Container plugins (Kontakt, Omnisphere)

normalization:
  input_to_device: actual = min + normalized * (max - min)
  device_to_output: normalized = (actual - min) / (max - min)
```

### 5.2 Macro Control for Container Plugins

```yaml
component: RSDV002A
name: container_plugin_macros
description: Standard macro access for Kontakt/Omnisphere/etc

known_containers:
  kontakt:
    macro_count: 8
    typical_indices: [1, 2, 3, 4, 5, 6, 7, 8]
    host_automation_slots: 16
  omnisphere:
    macro_count: 8
    orb_controls: 4
  serum:
    macro_count: 4
  massive_x:
    macro_count: 8

invariants:
  - macros expose full [0.0, 1.0] range
  - macro assignments defined by preset, not plugin
  - macro names may be custom-labeled by preset designer
```

---

## 6. Validation Framework

### 6.1 Pre-Execution Validation

Every operation must pass validation before execution:

```python
class ValidationResult:
    valid: bool
    errors: list[ValidationError]
    warnings: list[ValidationWarning]

class ValidationError:
    code: int        # Error code from operation spec
    message: str     # Human-readable
    param: str       # Parameter that failed
    constraint: str  # Violated constraint

def validate_operation(operation: str, params: dict) -> ValidationResult:
    """
    Validate all parameters against their bounded definitions.
    
    Returns ValidationResult with:
    - valid=True if all constraints satisfied
    - errors list if validation failed
    - warnings for soft constraints
    """
```

### 6.2 Error Code Ranges

| Range | Category | Description |
|-------|----------|-------------|
| 1000-1099 | Connection | Transport layer errors |
| 1100-1199 | Validation | Parameter validation failures |
| 1200-1299 | Ableton | Live API errors |
| 1300-1399 | Theory | Music theory computation errors |
| 1400-1499 | Automation | Automation operation errors |
| 1500-1599 | Arrangement | Arrangement operation errors |
| 1600-1699 | Browser | Browser/plugin loading errors |

### 6.3 Invariant Checking

```python
@dataclass
class InvariantCheck:
    name: str
    expression: str  # Mathematical expression
    check: Callable[[Any], bool]
    severity: Literal["error", "warning"]

# Example: Voice leading invariant
THVL001A_invariants = [
    InvariantCheck(
        name="max_voice_leap",
        expression="|Δpitch| ≤ 8",
        check=lambda trans: abs(trans.from_pitch - trans.to_pitch) <= 8,
        severity="warning"
    ),
    InvariantCheck(
        name="ascending_scale",
        expression="∀ i: notes[i] < notes[i+1]",
        check=lambda notes: all(notes[i] < notes[i+1] for i in range(len(notes)-1)),
        severity="error"
    ),
]
```

---

## 7. Circuit Breakers

### 7.1 Browser Scan Limits

```yaml
circuit_breaker: BRDS001A_scan_limit
triggers:
  - scan_depth > max_depth
  - items_scanned > 10000
  - elapsed_time > 30s
action: terminate_scan, return_partial_results
recovery: automatic_next_call
```

### 7.2 Automation Breakpoint Limits

```yaml
circuit_breaker: AUSP001A_breakpoint_limit
triggers:
  - breakpoint_count > 1000
  - invalid_time_sequence detected
action: reject_operation, return_error
recovery: user_must_fix_input
```

---

## 8. Audit Logging

### 8.1 Logged Events

| Event Type | Logged Data | Retention |
|------------|-------------|-----------|
| Operation Invoked | command, params, timestamp | 30 days |
| Validation Failure | command, params, errors | 30 days |
| Snapshot Created | snapshot_id, reason | Matches snapshot |
| Plugin Loaded | track, uri, device_name | 30 days |
| Automation Set | track, clip, param, breakpoint_count | 30 days |

### 8.2 Log Format

```python
logger.info(
    "Operation executed: {command} | "
    "Track={track_index} | "
    "Duration={duration_ms}ms | "
    "Result={result_status}",
    command=command,
    track_index=params.get("track_index"),
    duration_ms=elapsed_ms,
    result_status="success" if not error else "error"
)
```

---

## 9. Component Registry

### 9.1 Theory Components

| Code | Class/Function | Description | Reference |
|------|----------------|-------------|-----------|
| `THSC001A` | `get_scale_notes` | Scale generation | Music theory |
| `THCH001A` | `generate_progression` | Chord progression | Roman numeral analysis |
| `THVL001A` | `generate_voiced_progression` | Voice leading | Nearest-tone algorithm |
| `THML001A` | `generate_melody` | Melody generation | Contour-based |
| `THCA001A` | `create_cadence` | Cadence patterns | Music theory |
| `THOR001A` | `suggest_instruments` | Orchestration | Register analysis |

### 9.2 Automation Components

| Code | Class/Function | Description | Reference |
|------|----------------|-------------|-----------|
| `AUGT001A` | `_get_clip_automation` | Get envelope | Ableton API |
| `AUSP001A` | `_set_clip_automation` | Set breakpoints | Ableton API |
| `AUCL001A` | `_clear_clip_automation` | Clear envelope | Ableton API |
| `AUEN001A` | `create_automation_envelope` | Preset shapes | Linear/sine/etc |

### 9.3 Browser Components

| Code | Class/Function | Description | Reference |
|------|----------------|-------------|-----------|
| `BRSC001A` | `_get_browser_tree` | Tree scan | Ableton API |
| `BRSC002A` | `_get_browser_items_at_path` | Path navigation | Ableton API |
| `BRLD001A` | `_load_browser_item` | Load by URI | Ableton API |
| `BRDS001A` | `_discover_plugin_presets` | NKS discovery | Pattern matching |
| `BRFN001A` | `_find_browser_item_by_uri` | URI search | Recursive scan |

### 9.4 Arrangement Components

| Code | Class/Function | Description | Reference |
|------|----------------|-------------|-----------|
| `ARPC001A` | `_place_clip_in_arrangement` | Clip placement | Ableton API |
| `ARLC001A` | `_create_locator` | Cue points | Ableton API |
| `ARGL001A` | `_get_locators` | List locators | Ableton API |
| `ARJL001A` | `_jump_to_locator` | Navigate | Ableton API |
| `ARSL001A` | `_set_loop_region` | Loop bounds | Ableton API |
| `ARJB001A` | `_jump_to_bar` | Seek position | Ableton API |
| `ARDC001A` | `_duplicate_clip_in_arrangement` | Clone section | Ableton API |

---

## 10. Version Control

| Date | Version | Changes | Author |
|------|---------|---------|--------|
| 2025-12-22 | 1.0 | Initial operational governance document | Claude |
