# User Guide

*Practical Operation of Sunny*

## Overview

This guide explains how to use Sunny for music production with AI assistance. It assumes the system is installed and configured; see the README for installation instructions.

---

## Part I: Setup

### 1.1 Installation

Clone the repository and install dependencies:

```bash
git clone https://github.com/averagestudentdontfail/Sunny.git
cd Sunny
make install
```

### 1.2 Remote Script Installation

Copy the remote script to Ableton's User Library:

```bash
cp -r lib/remotescript ~/Music/Ableton/User\ Library/Remote\ Scripts/Sunny
```

On Windows:
```powershell
xcopy /E lib\remotescript "%APPDATA%\Ableton\Live x.x\Preferences\User Remote Scripts\Sunny"
```

### 1.3 Ableton Configuration

1. Open Ableton Live
2. Navigate to Preferences → Link, Tempo & MIDI
3. Under Control Surface, select "Sunny" from the dropdown
4. Configure Input and Output if prompted

### 1.4 MCP Configuration

Add Sunny to your AI assistant's MCP configuration:

```json
{
  "mcpServers": {
    "Sunny": {
      "command": "python",
      "args": ["-m", "sunny.server"],
      "cwd": "/path/to/Sunny"
    }
  }
}
```

---

## Part II: Theory Engine

### 2.1 Scales

Query scale notes by root and type:

```
get_scale_notes(root="C", scale_type="major")
→ [60, 62, 64, 65, 67, 69, 71]
```

Available scale types:

| Category | Scales |
|----------|--------|
| Church modes | major, minor, dorian, phrygian, lydian, mixolydian, locrian |
| Symmetric | whole_tone, octatonic_hw, octatonic_wh |
| Bebop | bebop_dominant, bebop_major, bebop_minor |
| Exotic | double_harmonic, hungarian_minor, phrygian_dominant |

### 2.2 Chord Generation

Generate chords by root and quality:

```
get_chord_notes(root="C", quality="major7")
→ [60, 64, 67, 71]
```

Available qualities: major, minor, diminished, augmented, major7, minor7, dominant7, diminished7, half_diminished7

### 2.3 Functional Harmony

Classify chords by function:

```
analyse_chord_function(chord=[60, 64, 67], key="C")
→ {"function": "T", "numeral": "I", "quality": "major"}
```

Generate chords by function:

```
get_chord_by_function(key="C", function="D")
→ {"root": "G", "quality": "major", "notes": [67, 71, 74]}
```

### 2.4 Voice Leading

Compute optimal voicing for a chord progression:

```
voice_lead(
    source=[60, 64, 67, 72],
    target_notes=[62, 65, 69],
    preserve_bass=True
)
→ [62, 65, 69, 74]
```

### 2.5 Orchestration

Get instrument suggestions by emotional intent:

```
suggest_instruments(emotion="heroic")
→ ["brass", "timpani", "low_strings"]
```

---

## Part III: Project Control

### 3.1 Track Operations

**List tracks:**
```
get_tracks()
→ [{"index": 0, "name": "Bass", "type": "midi"}, ...]
```

**Create track:**
```
create_track(name="Synth Lead", track_type="midi")
→ {"index": 5, "name": "Synth Lead"}
```

**Delete track:**
```
delete_track(index=5)
```

### 3.2 Clip Operations

**List clips:**
```
get_clips(track_index=0)
→ [{"slot": 0, "name": "Intro", "length": 8.0}, ...]
```

**Create clip:**
```
create_clip(track_index=0, slot=1, length=4.0, name="Verse")
```

**Add notes to clip:**
```
add_notes(
    track_index=0,
    slot=1,
    notes=[
        {"pitch": 60, "start": 0.0, "duration": 1.0, "velocity": 100},
        {"pitch": 64, "start": 1.0, "duration": 1.0, "velocity": 100}
    ]
)
```

### 3.3 Device Operations

**List devices:**
```
get_devices(track_index=0)
→ [{"index": 0, "name": "Wavetable", "class": "instrument"}, ...]
```

**Get parameters:**
```
get_parameters(track_index=0, device_index=0)
→ [{"index": 0, "name": "Filter Freq", "value": 0.5, "min": 0, "max": 1}, ...]
```

**Set parameter:**
```
set_parameter(track_index=0, device_index=0, param_index=0, value=0.75)
```

### 3.4 Transport Control

**Play/Stop:**
```
transport_play()
transport_stop()
```

**Set tempo:**
```
set_tempo(bpm=120)
```

**Set time signature:**
```
set_time_signature(numerator=4, denominator=4)
```

---

## Part IV: Snapshots

### 4.1 Creating Snapshots

Snapshots capture project state for later restoration:

```
create_snapshot(name="before_chorus_edit")
→ {"id": "snap_20260115_143022", "name": "before_chorus_edit"}
```

### 4.2 Listing Snapshots

```
list_snapshots()
→ [
    {"id": "snap_20260115_143022", "name": "before_chorus_edit", "created": "..."},
    {"id": "snap_20260115_140000", "name": "auto", "created": "..."}
]
```

### 4.3 Restoring Snapshots

```
restore_snapshot(snapshot_id="snap_20260115_143022")
```

### 4.4 Automatic Snapshots

The system creates automatic snapshots before destructive operations:
- Deleting tracks
- Clearing clips
- Resetting devices

---

## Part V: Common Workflows

### 5.1 Creating a Chord Progression

1. Query chords by function:
```
get_chord_by_function(key="C", function="T")  # I
get_chord_by_function(key="C", function="S")  # IV
get_chord_by_function(key="C", function="D")  # V
get_chord_by_function(key="C", function="T")  # I
```

2. Apply voice leading:
```
voice_lead(source=tonic, target_notes=subdominant_notes)
voice_lead(source=subdominant, target_notes=dominant_notes)
voice_lead(source=dominant, target_notes=tonic_notes)
```

3. Create a clip with the progression:
```
create_clip(track_index=0, slot=0, length=8.0)
add_notes(track_index=0, slot=0, notes=progression_notes)
```

### 5.2 Transposing a Melody

1. Get current notes:
```
get_clip_notes(track_index=0, slot=0)
```

2. Quantise to new scale:
```
for note in notes:
    quantise_to_scale(pitch=note.pitch, root="G", scale="major")
```

3. Update clip:
```
clear_clip(track_index=0, slot=0)
add_notes(track_index=0, slot=0, notes=transposed_notes)
```

### 5.3 Automating a Parameter

1. Identify the parameter:
```
get_parameters(track_index=2, device_index=0)
```

2. Set values over time using the UDP channel for low latency

---

## Part VI: Troubleshooting

### 6.1 Connection Issues

**Symptom:** Server cannot connect to Ableton

**Solutions:**
- Verify the remote script is installed correctly
- Check that Sunny is selected as a control surface
- Restart Ableton Live
- Check firewall settings for ports 9000 (TCP) and 9001 (UDP)

### 6.2 Theory Engine Errors

**Symptom:** Scale or chord queries return errors

**Solutions:**
- Verify root note format ("C", "F#", "Bb")
- Check scale/chord type spelling matches available options
- Consult the foundations document for available types

### 6.3 Latency Issues

**Symptom:** Parameter changes feel delayed

**Solutions:**
- Ensure UDP transport is active
- Check for network congestion
- Reduce Ableton buffer size

### 6.4 Snapshot Restoration Fails

**Symptom:** Cannot restore previous state

**Solutions:**
- Verify snapshot exists with list_snapshots
- Check snapshot directory permissions
- Ensure sufficient disk space

---

## Part VII: Environment Variables

| Variable | Purpose | Default |
|----------|---------|---------|
| `SUNNY_LOG_LEVEL` | Logging verbosity | `INFO` |
| `SUNNY_SNAPSHOT_DIR` | Snapshot storage | `~/.sunny/snapshots` |
| `SUNNY_MAX_SNAPSHOTS` | Retention limit | `50` |
| `SUNNY_TCP_PORT` | TCP transport port | `9000` |
| `SUNNY_UDP_PORT` | UDP transport port | `9001` |

Create a `.env` file in the project root to set these values.

---

## Appendix: Quick Reference

### Theory Functions

| Function | Purpose |
|----------|---------|
| `get_scale_notes` | Notes in a scale |
| `get_chord_notes` | Notes in a chord |
| `analyse_chord_function` | Classify chord by function |
| `voice_lead` | Compute smooth voicing |
| `suggest_instruments` | Orchestration by emotion |

### Project Functions

| Function | Purpose |
|----------|---------|
| `get_tracks` | List all tracks |
| `create_track` | Add a track |
| `get_clips` | List clips in a track |
| `create_clip` | Add a clip |
| `add_notes` | Insert notes in a clip |
| `set_parameter` | Adjust device parameter |

### Snapshot Functions

| Function | Purpose |
|----------|---------|
| `create_snapshot` | Capture current state |
| `list_snapshots` | Show available snapshots |
| `restore_snapshot` | Revert to saved state |
