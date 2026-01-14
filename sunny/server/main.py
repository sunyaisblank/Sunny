"""Sunny - FastMCP Server.

Component: SRMN001A
Domain: SR (Server) | Category: MN (Main)

Main entry point for the MCP server that provides AI agents
with comprehensive control over Ableton Live.

Error Codes:
    - 1000: Server initialization failed
    - 1001: Ableton connection failed
"""

from __future__ import annotations

import json
import logging
from contextlib import asynccontextmanager
from typing import TYPE_CHECKING, Any, AsyncIterator

from mcp.server.fastmcp import FastMCP, Context

# Configure logging
logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    level=logging.INFO
)
logger = logging.getLogger("sunny")


# =============================================================================
# Lifespan Management
# =============================================================================

@asynccontextmanager
async def lifespan(server: FastMCP) -> AsyncIterator[dict[str, Any]]:
    """Manage server lifecycle and shared resources.
    
    Yields:
        Dictionary containing shared resources (Ableton connection, theory engine)
    """
    logger.info("Sunny starting...")
    
    # Import here to avoid circular imports
    from Sunny.Server.transport import AbletonConnection
    from Sunny.Theory.engine import TheoryEngine
    from Sunny.Server.snapshot import SnapshotManager
    
    # Initialize shared resources
    ableton = AbletonConnection()
    theory = TheoryEngine()
    snapshots = SnapshotManager()
    
    try:
        # Attempt connection to Ableton
        connected = await ableton.connect()
        if connected:
            logger.info("Connected to Ableton Live")
        else:
            logger.warning("Could not connect to Ableton Live - running in offline mode")
        
        yield {
            "ableton": ableton,
            "theory": theory,
            "snapshots": snapshots,
        }
    finally:
        await ableton.disconnect()
        logger.info("Sunny stopped")


# =============================================================================
# MCP Server Instance
# =============================================================================

mcp = FastMCP(
    "Sunny",
    lifespan=lifespan,
)


# =============================================================================
# Helper Functions
# =============================================================================

def get_ableton(ctx: Context) -> Any:
    """Get Ableton connection from context."""
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")
    
    ableton = lifespan_ctx.get("ableton")
    if ableton is None:
        raise RuntimeError("Ableton connection not available")
    
    return ableton


def get_theory(ctx: Context) -> Any:
    """Get theory engine from context."""
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")
    
    theory = lifespan_ctx.get("theory")
    if theory is None:
        raise RuntimeError("Theory engine not available")
    
    return theory


def get_snapshots(ctx: Context) -> Any:
    """Get snapshot manager from context."""
    lifespan_ctx = ctx.request_context.lifespan_context
    if lifespan_ctx is None:
        raise RuntimeError("Lifespan context not initialized - server may still be starting")
    
    snapshots = lifespan_ctx.get("snapshots")
    if snapshots is None:
        raise RuntimeError("Snapshot manager not available")
    
    return snapshots


# =============================================================================
# Session Tools
# =============================================================================

@mcp.tool()
async def get_session_info(ctx: Context) -> str:
    """Get detailed information about the current Ableton session.
    
    Returns information including:
    - Tempo (BPM)
    - Time signature
    - Track count (MIDI, Audio, Return, Master)
    - Current playback state
    - Global scale/key (if set)
    
    Returns:
        JSON string containing session information
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_session_info")
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting session info: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_tempo(ctx: Context, bpm: float) -> str:
    """Set the project tempo in BPM.
    
    Args:
        bpm: Tempo in beats per minute (20.0 to 999.0)
    
    Returns:
        Confirmation message
    """
    if not 20.0 <= bpm <= 999.0:
        return json.dumps({"error": "BPM must be between 20.0 and 999.0"})
    
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_tempo", {"bpm": bpm})
        return f"Tempo set to {bpm} BPM"
    except Exception as e:
        logger.error(f"Error setting tempo: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Track Tools
# =============================================================================

@mcp.tool()
async def create_midi_track(
    ctx: Context,
    name: str | None = None,
    index: int = -1
) -> str:
    """Create a new MIDI track in the session.
    
    Args:
        name: Optional name for the track
        index: Position to insert (-1 = end of track list)
    
    Returns:
        JSON with created track info including index and name
    """
    try:
        ableton = get_ableton(ctx)
        params = {"index": index}
        if name:
            params["name"] = name
        
        result = await ableton.send_command("create_midi_track", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating MIDI track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_audio_track(
    ctx: Context,
    name: str | None = None,
    index: int = -1
) -> str:
    """Create a new audio track in the session.
    
    Args:
        name: Optional name for the track
        index: Position to insert (-1 = end of track list)
    
    Returns:
        JSON with created track info including index and name
    """
    try:
        ableton = get_ableton(ctx)
        params = {"index": index}
        if name:
            params["name"] = name
        
        result = await ableton.send_command("create_audio_track", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating audio track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def delete_track(ctx: Context, track_index: int) -> str:
    """Delete a track from the session.
    
    SAFETY: Automatically creates a project snapshot before deletion.
    
    Args:
        track_index: Index of the track to delete
    
    Returns:
        Confirmation with snapshot ID for recovery
    """
    try:
        # Create snapshot before destructive operation
        snapshots = get_snapshots(ctx)
        snapshot_id = await snapshots.create_snapshot(f"Before delete track {track_index}")
        
        ableton = get_ableton(ctx)
        result = await ableton.send_command("delete_track", {"track_index": track_index})
        
        return json.dumps({
            "status": "success",
            "message": f"Deleted track {track_index}",
            "snapshot_id": snapshot_id,
            "recovery": f"Use restore_snapshot('{snapshot_id}') to undo"
        }, indent=2)
    except Exception as e:
        logger.error(f"Error deleting track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_track_info(ctx: Context, track_index: int) -> str:
    """Get detailed information about a specific track.
    
    Args:
        track_index: Index of the track to query
    
    Returns:
        JSON with track details including:
        - Name
        - Type (MIDI/Audio)
        - Volume/Pan
        - Mute/Solo/Arm state
        - Devices loaded
        - Clip slots
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_track_info", {"track_index": track_index})
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting track info: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Theory Tools
# =============================================================================

@mcp.tool()
async def generate_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str],
    octave: int = 4
) -> str:
    """Generate a chord progression from Roman numerals.
    
    Uses music theory to create proper chord voicings based on
    the specified key and scale/mode.
    
    Args:
        root: Root note (e.g., "C", "F#", "Bb")
        scale: Scale/mode name (e.g., "major", "minor", "dorian", "phrygian")
        numerals: List of Roman numerals (e.g., ["ii", "V", "I"])
        octave: Base octave for chord voicings (default: 4)
    
    Returns:
        JSON array of chord objects, each containing:
        - numeral: The Roman numeral
        - root: Chord root note
        - quality: Chord quality (major, minor, diminished, etc.)
        - notes: MIDI note numbers
    
    Example:
        generate_progression("C", "major", ["ii", "V", "I"])
        Returns: Dm, G, C chords with MIDI notes
    """
    try:
        theory = get_theory(ctx)
        logger.info(f"Generating progression: {root} {scale} {numerals}")
        progression = theory.generate_progression(root, scale, numerals, octave)
        logger.info(f"Generated {len(progression)} chords")
        
        if not progression:
            logger.warning(f"Empty progression for {numerals} in {root} {scale}")
        
        return json.dumps(progression, indent=2)
    except ValueError as e:
        logger.warning(f"Invalid theory input: {e}")
        return json.dumps({"error": f"Invalid theory input: {e}"})
    except Exception as e:
        logger.error(f"Error generating progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_scale_notes(
    ctx: Context,
    root: str,
    mode: str,
    octave: int = 4
) -> str:
    """Get MIDI note numbers for a scale.
    
    Args:
        root: Root note (e.g., "C", "F#")
        mode: Scale mode (ionian, dorian, phrygian, lydian, 
              mixolydian, aeolian, locrian)
        octave: Octave for the scale (default: 4)
    
    Returns:
        JSON array of MIDI note numbers for scale degrees 1-7
    """
    try:
        theory = get_theory(ctx)
        notes = theory.get_scale_notes(root, mode, octave)
        return json.dumps({
            "root": root,
            "mode": mode,
            "octave": octave,
            "notes": notes,
        }, indent=2)
    except ValueError as e:
        return json.dumps({"error": f"Invalid scale: {e}"})
    except Exception as e:
        logger.error(f"Error getting scale notes: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Clip Tools
# =============================================================================

@mcp.tool()
async def create_clip(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    length_beats: float = 4.0,
    name: str | None = None
) -> str:
    """Create a new MIDI clip in the specified track and slot.
    
    Args:
        track_index: Index of the MIDI track
        clip_slot: Clip slot index (row in session view)
        length_beats: Length of the clip in beats (default: 4.0)
        name: Optional name for the clip
    
    Returns:
        JSON with created clip info
    """
    try:
        ableton = get_ableton(ctx)
        params = {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "length": length_beats,
        }
        if name:
            params["name"] = name
        
        result = await ableton.send_command("create_clip", params)
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error creating clip: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def add_notes_to_clip(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    notes: list[dict]
) -> str:
    """Add MIDI notes to an existing clip.
    
    Args:
        track_index: Index of the track containing the clip
        clip_slot: Clip slot index
        notes: List of note dictionaries, each containing:
            - pitch: MIDI note number (0-127)
            - start_time: Start position in beats
            - duration: Note length in beats
            - velocity: Note velocity (1-127, default: 100)
            - mute: Whether note is muted (default: false)
    
    Returns:
        Confirmation with number of notes added
    
    Example:
        add_notes_to_clip(0, 0, [
            {"pitch": 60, "start_time": 0, "duration": 1, "velocity": 100},
            {"pitch": 64, "start_time": 1, "duration": 1, "velocity": 90},
        ])
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("add_notes_to_clip", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "notes": notes,
        })
        return f"Added {len(notes)} notes to clip at track {track_index}, slot {clip_slot}"
    except Exception as e:
        logger.error(f"Error adding notes to clip: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Arrangement View Tools
# =============================================================================

@mcp.tool()
async def place_clip_in_arrangement(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    bar: int,
    beat: float = 1.0
) -> str:
    """Place a clip from session view into the arrangement view at a specific position.
    
    Args:
        track_index: Index of the track containing the clip
        clip_slot: Clip slot index in session view
        bar: Bar number to place the clip (1-indexed)
        beat: Beat within the bar (default: 1.0 = first beat)
    
    Returns:
        Confirmation with placement position
    """
    try:
        ableton = get_ableton(ctx)
        
        # Convert bar/beat to beats from start
        # Assuming 4/4 time signature for now
        position_beats = (bar - 1) * 4 + (beat - 1)
        
        result = await ableton.send_command("place_clip_in_arrangement", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "position": position_beats,
        })
        return json.dumps({
            "status": "success",
            "message": f"Placed clip at bar {bar}, beat {beat}",
            "position_beats": position_beats,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error placing clip in arrangement: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_locator(
    ctx: Context,
    name: str,
    bar: int,
    beat: float = 1.0
) -> str:
    """Create a locator (marker) at a specific position in the arrangement.
    
    Args:
        name: Name for the locator (e.g., "Verse 1", "Chorus", "Bridge")
        bar: Bar number for the locator (1-indexed)
        beat: Beat within the bar (default: 1.0)
    
    Returns:
        JSON with locator info
    """
    try:
        ableton = get_ableton(ctx)
        
        position_beats = (bar - 1) * 4 + (beat - 1)
        
        result = await ableton.send_command("create_locator", {
            "name": name,
            "position": position_beats,
        })
        return json.dumps({
            "status": "success",
            "name": name,
            "bar": bar,
            "beat": beat,
            "position_beats": position_beats,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error creating locator: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_locators(ctx: Context) -> str:
    """Get all locators in the arrangement.
    
    Returns:
        JSON array of locators with names and positions
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_locators")
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting locators: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def jump_to_locator(ctx: Context, name: str) -> str:
    """Jump to a named locator in the arrangement.
    
    Args:
        name: Name of the locator to jump to
    
    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("jump_to_locator", {"name": name})
        return f"Jumped to locator: {name}"
    except Exception as e:
        logger.error(f"Error jumping to locator: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_loop_region(
    ctx: Context,
    start_bar: int,
    end_bar: int,
    start_beat: float = 1.0,
    end_beat: float = 1.0
) -> str:
    """Set the loop region in the arrangement.
    
    Args:
        start_bar: Starting bar number (1-indexed)
        end_bar: Ending bar number (1-indexed)
        start_beat: Starting beat within bar (default: 1.0)
        end_beat: Ending beat within bar (default: 1.0)
    
    Returns:
        Confirmation with loop region details
    """
    try:
        ableton = get_ableton(ctx)
        
        start_beats = (start_bar - 1) * 4 + (start_beat - 1)
        end_beats = (end_bar - 1) * 4 + (end_beat - 1)
        
        result = await ableton.send_command("set_loop_region", {
            "start": start_beats,
            "end": end_beats,
        })
        return json.dumps({
            "status": "success",
            "loop_start": f"Bar {start_bar}, Beat {start_beat}",
            "loop_end": f"Bar {end_bar}, Beat {end_beat}",
            "length_bars": end_bar - start_bar,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error setting loop region: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def duplicate_clip_in_arrangement(
    ctx: Context,
    track_index: int,
    source_bar: int,
    target_bar: int,
    length_bars: int = 4
) -> str:
    """Duplicate a section of the arrangement.
    
    Args:
        track_index: Track to duplicate from
        source_bar: Starting bar of the section to duplicate
        target_bar: Bar to paste the duplicated section
        length_bars: Length in bars to duplicate (default: 4)
    
    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        
        source_beats = (source_bar - 1) * 4
        target_beats = (target_bar - 1) * 4
        length_beats = length_bars * 4
        
        result = await ableton.send_command("duplicate_arrangement_region", {
            "track_index": track_index,
            "source_position": source_beats,
            "target_position": target_beats,
            "length": length_beats,
        })
        return json.dumps({
            "status": "success",
            "message": f"Duplicated {length_bars} bars from bar {source_bar} to bar {target_bar}",
        }, indent=2)
    except Exception as e:
        logger.error(f"Error duplicating arrangement: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Transport Tools
# =============================================================================

@mcp.tool()
async def play(ctx: Context) -> str:
    """Start playback from current position.
    
    Returns:
        Playback status
    """
    try:
        ableton = get_ableton(ctx)
        await ableton.send_command("start_playback")
        return "Playback started"
    except Exception as e:
        logger.error(f"Error starting playback: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def stop(ctx: Context) -> str:
    """Stop playback.
    
    Returns:
        Playback status
    """
    try:
        ableton = get_ableton(ctx)
        await ableton.send_command("stop_playback")
        return "Playback stopped"
    except Exception as e:
        logger.error(f"Error stopping playback: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def jump_to_bar(ctx: Context, bar: int, beat: float = 1.0) -> str:
    """Jump to a specific bar position.
    
    Args:
        bar: Bar number to jump to (1-indexed)
        beat: Beat within the bar (default: 1.0)
    
    Returns:
        New position
    """
    try:
        ableton = get_ableton(ctx)
        position_beats = (bar - 1) * 4 + (beat - 1)
        await ableton.send_command("set_position", {"position": position_beats})
        return f"Jumped to bar {bar}, beat {beat}"
    except Exception as e:
        logger.error(f"Error jumping to position: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Mixer Tools
# =============================================================================

@mcp.tool()
async def set_track_volume(
    ctx: Context,
    track_index: int,
    volume_db: float
) -> str:
    """Set track volume in dB.
    
    Args:
        track_index: Index of the track
        volume_db: Volume in dB (-inf to +6)
    
    Returns:
        Confirmation with new volume
    """
    try:
        ableton = get_ableton(ctx)
        
        # Convert dB to normalized 0-1 value
        # Ableton's volume range is -inf to +6dB
        # Approximate: 0dB ≈ 0.85, -6dB ≈ 0.7, +6dB = 1.0
        if volume_db <= -70:
            normalized = 0.0
        else:
            # Linear approximation
            normalized = max(0.0, min(1.0, (volume_db + 70) / 76))
        
        result = await ableton.send_command("set_track_volume", {
            "track_index": track_index,
            "volume": normalized,
        })
        return f"Track {track_index} volume set to {volume_db} dB"
    except Exception as e:
        logger.error(f"Error setting track volume: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_track_pan(
    ctx: Context,
    track_index: int,
    pan: float
) -> str:
    """Set track panning.
    
    Args:
        track_index: Index of the track
        pan: Pan position (-1.0 = left, 0.0 = center, 1.0 = right)
    
    Returns:
        Confirmation with new pan position
    """
    try:
        ableton = get_ableton(ctx)
        
        pan = max(-1.0, min(1.0, pan))
        
        result = await ableton.send_command("set_track_pan", {
            "track_index": track_index,
            "pan": pan,
        })
        
        pan_str = "center" if abs(pan) < 0.05 else f"{'left' if pan < 0 else 'right'} {abs(int(pan * 50))}"
        return f"Track {track_index} pan set to {pan_str}"
    except Exception as e:
        logger.error(f"Error setting track pan: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def mute_track(ctx: Context, track_index: int, mute: bool = True) -> str:
    """Mute or unmute a track.
    
    Args:
        track_index: Index of the track
        mute: True to mute, False to unmute (default: True)
    
    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_track_mute", {
            "track_index": track_index,
            "mute": mute,
        })
        return f"Track {track_index} {'muted' if mute else 'unmuted'}"
    except Exception as e:
        logger.error(f"Error muting track: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def solo_track(ctx: Context, track_index: int, solo: bool = True) -> str:
    """Solo or unsolo a track.
    
    Args:
        track_index: Index of the track
        solo: True to solo, False to unsolo (default: True)
    
    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_track_solo", {
            "track_index": track_index,
            "solo": solo,
        })
        return f"Track {track_index} {'soloed' if solo else 'unsoloed'}"
    except Exception as e:
        logger.error(f"Error soloing track: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Device Tools (VST/Plugin Parameter Control)
# =============================================================================

@mcp.tool()
async def get_device_parameters(
    ctx: Context,
    track_index: int,
    device_index: int = 0
) -> str:
    """Get all parameters for a device (including VST/AU plugins).
    
    This is essential for discovering VST plugin parameters. Each VST exposes
    its automatable parameters through this interface.
    
    Args:
        track_index: Index of the track containing the device
        device_index: Index of the device on the track (default: 0 = first device)
    
    Returns:
        JSON with device name and array of parameters, each containing:
        - index: Parameter index (use this for set_device_parameter)
        - name: Parameter name (e.g., "Filter Cutoff", "Osc A Volume")
        - value: Current value
        - min: Minimum value
        - max: Maximum value
        - is_quantized: Whether parameter uses discrete steps
    
    Example output for Sylenth1:
        {
            "device_name": "Sylenth1",
            "parameters": [
                {"index": 0, "name": "Device On", "value": 1.0, "min": 0.0, "max": 1.0},
                {"index": 1, "name": "Preset", "value": 0.5, ...},
                {"index": 2, "name": "Master Volume", ...},
                ...
            ]
        }
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_device_parameters", {
            "track_index": track_index,
            "device_index": device_index,
        })
        
        # Format output for readability
        device_name = result.get("device_name", "Unknown")
        params = result.get("parameters", [])
        
        output = f"Device: {device_name}\n"
        output += f"Total Parameters: {len(params)}\n\n"
        
        # Group parameters for easier reading
        for param in params[:50]:  # Limit to first 50 for readability
            output += f"  [{param['index']:3d}] {param['name']}: {param['value']:.3f} (range: {param['min']:.1f} - {param['max']:.1f})\n"
        
        if len(params) > 50:
            output += f"\n  ... and {len(params) - 50} more parameters\n"
        
        output += f"\nFull JSON:\n{json.dumps(result, indent=2)}"
        return output
    except Exception as e:
        logger.error(f"Error getting device parameters: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_device_parameter(
    ctx: Context,
    track_index: int,
    device_index: int,
    param_index: int,
    value: float
) -> str:
    """Set a device parameter value (works with VST/AU plugins).
    
    Use get_device_parameters first to discover available parameters and their ranges.
    
    Args:
        track_index: Index of the track containing the device
        device_index: Index of the device on the track
        param_index: Index of the parameter (from get_device_parameters)
        value: New value (will be clamped to parameter's min/max range)
    
    Returns:
        Confirmation with parameter name and new value
    
    Example:
        # Set Sylenth1's filter cutoff (assuming param_index 15)
        set_device_parameter(track_index=0, device_index=0, param_index=15, value=0.75)
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_device_parameter", {
            "track_index": track_index,
            "device_index": device_index,
            "param_index": param_index,
            "value": value,
        })
        
        param_name = result.get("param_name", "Unknown")
        new_value = result.get("value", value)
        return f"Set '{param_name}' to {new_value}"
    except Exception as e:
        logger.error(f"Error setting device parameter: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Snapshot Tools
# =============================================================================

@mcp.tool()
async def create_snapshot(ctx: Context, reason: str) -> str:
    """Create a project snapshot for later recovery.
    
    Args:
        reason: Description of why the snapshot was created
    
    Returns:
        Snapshot ID that can be used for restoration
    """
    try:
        snapshots = get_snapshots(ctx)
        snapshot_id = await snapshots.create_snapshot(reason)
        return json.dumps({
            "snapshot_id": snapshot_id,
            "reason": reason,
            "message": "Snapshot created successfully"
        }, indent=2)
    except Exception as e:
        logger.error(f"Error creating snapshot: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_snapshots(ctx: Context) -> str:
    """List all available project snapshots.
    
    Returns:
        JSON array of snapshots with IDs, timestamps, and reasons
    """
    try:
        snapshots = get_snapshots(ctx)
        snapshot_list = await snapshots.list_snapshots()
        return json.dumps(snapshot_list, indent=2)
    except Exception as e:
        logger.error(f"Error listing snapshots: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def restore_snapshot(ctx: Context, snapshot_id: str) -> str:
    """Restore the project to a previous snapshot state.
    
    Args:
        snapshot_id: ID of the snapshot to restore
    
    Returns:
        Confirmation of restoration
    """
    try:
        snapshots = get_snapshots(ctx)
        await snapshots.restore_snapshot(snapshot_id)
        return json.dumps({
            "status": "success",
            "message": f"Restored snapshot {snapshot_id}"
        }, indent=2)
    except Exception as e:
        logger.error(f"Error restoring snapshot: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Advanced Theory Tools
# =============================================================================

@mcp.tool()
async def analyze_progression_functions(
    ctx: Context,
    progression: list[str],
    mode: str = "major"
) -> str:
    """Analyze the functional harmony of a chord progression.
    
    Determines the harmonic function (Tonic/Subdominant/Dominant) 
    and tension level of each chord.
    
    Args:
        progression: List of Roman numerals (e.g., ["ii", "V", "I"])
        mode: "major" or "minor"
    
    Returns:
        JSON array with function (T/S/D) and tension (0-2) for each chord
    """
    try:
        theory = get_theory(ctx)
        analysis = theory.analyze_progression_functions(progression, mode)
        return json.dumps(analysis, indent=2)
    except Exception as e:
        logger.error(f"Error analyzing progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_negative_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str]
) -> str:
    """Generate the negative harmony (mirror) version of a progression.
    
    Negative harmony mirrors chords around the tonic-dominant axis,
    transforming major to minor while maintaining harmonic function.
    Used by Jacob Collier and film composers for dark transformations.
    
    Args:
        root: Key root note (e.g., "C")
        scale: Scale/mode name
        numerals: Roman numerals to mirror (e.g., ["ii", "V", "I"])
    
    Returns:
        JSON array of mirrored chord info
    
    Example:
        ii-V-I in C major → bVII-iv-i (shadow version)
    """
    try:
        theory = get_theory(ctx)
        mirrored = theory.generate_negative_progression(root, scale, numerals)
        return json.dumps(mirrored, indent=2)
    except Exception as e:
        logger.error(f"Error generating negative progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def add_secondary_dominant(
    ctx: Context,
    progression: list[str],
    before_numeral: str
) -> str:
    """Insert a secondary dominant chord before a target chord.
    
    Secondary dominants (V/x) create chromatic tension approaching
    non-tonic chords. Essential for jazz and romantic harmony.
    
    Args:
        progression: Original progression numerals
        before_numeral: Chord to approach with secondary dominant
    
    Returns:
        Updated progression with V/x inserted
    
    Example:
        ["I", "ii", "V", "I"] with before_numeral="V"
        → ["I", "ii", "V/V", "V", "I"]
    """
    try:
        theory = get_theory(ctx)
        new_progression = theory.add_secondary_dominant(progression, before_numeral)
        return json.dumps({"progression": new_progression}, indent=2)
    except Exception as e:
        logger.error(f"Error adding secondary dominant: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_borrowed_chords(
    ctx: Context,
    key: str,
    mode: str = "major"
) -> str:
    """Get chords available through modal interchange.
    
    Modal interchange borrows chords from the parallel mode.
    In major, common borrowed chords include iv, bVI, bVII.
    
    Args:
        key: Key root (e.g., "C")
        mode: Current mode ("major" or "minor")
    
    Returns:
        JSON array of borrowable chords with numerals and source mode
    """
    try:
        theory = get_theory(ctx)
        borrowed = theory.get_borrowed_chords(key, mode)
        return json.dumps(borrowed, indent=2)
    except Exception as e:
        logger.error(f"Error getting borrowed chords: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_voiced_progression(
    ctx: Context,
    root: str,
    scale: str,
    numerals: list[str],
    octave: int = 4
) -> str:
    """Generate a progression with smooth voice leading.
    
    Uses the nearest-tone algorithm to minimize voice movement
    between chords, avoiding large leaps and creating professional
    part writing.
    
    Args:
        root: Key root note
        scale: Scale/mode name
        numerals: Roman numeral sequence
        octave: Base octave
    
    Returns:
        JSON array of chords with both block and voiced notes
    """
    try:
        theory = get_theory(ctx)
        progression = theory.generate_progression_voiced(root, scale, numerals, octave)
        return json.dumps(progression, indent=2)
    except Exception as e:
        logger.error(f"Error generating voiced progression: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_cadence(
    ctx: Context,
    cadence_type: str,
    key: str,
    mode: str = "major",
    octave: int = 4
) -> str:
    """Create a specific cadence type.
    
    Available types: perfect_authentic, imperfect_authentic, half,
    plagal, deceptive, picardy, neapolitan, phrygian, backdoor,
    minor_plagal
    
    Args:
        cadence_type: Type of cadence
        key: Key root note
        mode: "major" or "minor"
        octave: Base octave
    
    Returns:
        JSON array of chords forming the cadence
    """
    try:
        theory = get_theory(ctx)
        cadence = theory.create_cadence(cadence_type, key, mode, octave)
        return json.dumps(cadence, indent=2)
    except Exception as e:
        logger.error(f"Error creating cadence: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_cadence_types(ctx: Context) -> str:
    """List all available cadence types with descriptions.
    
    Returns:
        JSON array of cadence types with numerals and emotional qualities
    """
    try:
        theory = get_theory(ctx)
        types = theory.list_cadence_types()
        return json.dumps(types, indent=2)
    except Exception as e:
        logger.error(f"Error listing cadence types: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def generate_melody(
    ctx: Context,
    root: str,
    scale: str,
    length: int = 8,
    octave: int = 5,
    contour: str = "arch"
) -> str:
    """Generate a scale-aware melody.
    
    Creates melodic lines that respect the given scale with
    natural-feeling contours and dynamics.
    
    Args:
        root: Scale root note (e.g., "C", "F#")
        scale: Scale/mode name (e.g., "major", "dorian", "phrygian_dominant")
        length: Number of notes to generate (default: 8)
        octave: Base octave (default: 5 for singable range)
        contour: Shape of melody ("arch", "ascending", "descending", "wave", "random")
    
    Returns:
        JSON array of note events with pitch, start_time, duration, velocity
    """
    try:
        theory = get_theory(ctx)
        melody = theory.generate_melody(root, scale, length, octave, None, contour)
        return json.dumps(melody, indent=2)
    except Exception as e:
        logger.error(f"Error generating melody: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_available_scales(ctx: Context) -> str:
    """List all available scales and modes.
    
    Returns over 40 scales including church modes, exotic scales,
    symmetric scales, and bebop scales.
    
    Returns:
        JSON array of scale names
    """
    try:
        theory = get_theory(ctx)
        scales = theory.get_available_scales()
        return json.dumps({
            "count": len(scales),
            "scales": sorted(scales)
        }, indent=2)
    except Exception as e:
        logger.error(f"Error listing scales: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_scale_info(ctx: Context, scale_name: str) -> str:
    """Get detailed information about a specific scale.
    
    Args:
        scale_name: Name of the scale (e.g., "phrygian_dominant", "whole_tone")
    
    Returns:
        JSON with intervals, chord qualities, and description
    """
    try:
        theory = get_theory(ctx)
        info = theory.get_scale_info(scale_name)
        if info is None:
            return json.dumps({"error": f"Scale '{scale_name}' not found"})
        return json.dumps(info, indent=2)
    except Exception as e:
        logger.error(f"Error getting scale info: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Orchestration Tools
# =============================================================================

@mcp.tool()
async def suggest_instruments(
    ctx: Context,
    emotional_color: str,
    register: str | None = None,
    category: str | None = None,
    limit: int = 5
) -> str:
    """Suggest orchestral instruments for a given emotional context.
    
    Returns instruments that best convey the specified emotional quality,
    optionally filtered by voice register or instrument category.
    
    Args:
        emotional_color: Emotion to convey (heroic, melancholy, ethereal,
            climactic, tension, pastoral, aggressive, mysterious, romantic, playful)
        register: Optional voice register filter (bass, tenor, alto, soprano, super)
        category: Optional category filter (strings, woodwinds, brass, percussion,
            keyboards, synths)
        limit: Maximum number of suggestions (default: 5)
    
    Returns:
        JSON array of instrument suggestions with rationale
    """
    try:
        from Sunny.Theory.orchestration import OrchestrationGuide
        guide = OrchestrationGuide()
        suggestions = guide.suggest_instruments(emotional_color, register, category, limit)
        return json.dumps(suggestions, indent=2)
    except Exception as e:
        logger.error(f"Error suggesting instruments: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_voice_assignment(
    ctx: Context,
    chord_notes: list[int]
) -> str:
    """Assign chord tones to voice roles with instrument suggestions.
    
    Analyzes chord voicing and suggests appropriate instruments
    for each voice based on register and orchestral practice.
    
    Args:
        chord_notes: List of MIDI note numbers forming the chord
    
    Returns:
        JSON with voice assignments (bass, tenor, alto, soprano)
        and suggested instruments for each
    """
    try:
        from Sunny.Theory.orchestration import OrchestrationGuide
        guide = OrchestrationGuide()
        assignment = guide.get_voice_assignment(chord_notes)
        return json.dumps(assignment, indent=2)
    except Exception as e:
        logger.error(f"Error assigning voices: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_emotional_colors(ctx: Context) -> str:
    """List all available emotional colors for orchestration.
    
    Returns each emotional color with sample instruments that
    typically convey that emotion.
    
    Returns:
        JSON array of emotional colors with sample instruments
    """
    try:
        from Sunny.Theory.orchestration import OrchestrationGuide
        guide = OrchestrationGuide()
        colors = guide.list_emotional_colors()
        return json.dumps(colors, indent=2)
    except Exception as e:
        logger.error(f"Error listing colors: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def list_orchestral_instruments(
    ctx: Context,
    category: str | None = None
) -> str:
    """List available orchestral and electronic instruments.
    
    Provides a database of instruments with their registers,
    categories, and descriptions.
    
    Args:
        category: Optional filter (strings, woodwinds, brass, percussion,
            keyboards, synths, vocals)
    
    Returns:
        JSON array of instruments with details
    """
    try:
        from Sunny.Theory.orchestration import OrchestrationGuide
        guide = OrchestrationGuide()
        instruments = guide.list_instruments(category)
        return json.dumps(instruments, indent=2)
    except Exception as e:
        logger.error(f"Error listing instruments: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Automation Tools
# =============================================================================

@mcp.tool()
async def get_clip_automation(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    parameter_path: str
) -> str:
    """Get automation breakpoints for a clip parameter.
    
    Args:
        track_index: Track containing the clip
        clip_slot: Clip slot index
        parameter_path: Device parameter path (e.g., "devices/0/parameters/1")
    
    Returns:
        JSON with automation envelope breakpoints
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_clip_automation", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "parameter_path": parameter_path,
        })
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting clip automation: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def set_clip_automation(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    parameter_path: str,
    breakpoints: list[dict]
) -> str:
    """Set automation breakpoints for a clip parameter.
    
    Creates or updates an automation envelope with specified breakpoints.
    
    Args:
        track_index: Track containing the clip
        clip_slot: Clip slot index
        parameter_path: Device parameter path
        breakpoints: List of {time, value} points where:
            - time: Position in beats (0.0 = clip start)
            - value: Normalized value (0.0 to 1.0)
    
    Returns:
        Confirmation with number of breakpoints set
    
    Example:
        set_clip_automation(0, 0, "devices/0/parameters/1", [
            {"time": 0.0, "value": 0.0},
            {"time": 2.0, "value": 1.0},
            {"time": 4.0, "value": 0.5},
        ])
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("set_clip_automation", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "parameter_path": parameter_path,
            "breakpoints": breakpoints,
        })
        return json.dumps({
            "status": "success",
            "breakpoints_set": len(breakpoints),
            "parameter": parameter_path,
        }, indent=2)
    except Exception as e:
        logger.error(f"Error setting clip automation: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def clear_clip_automation(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    parameter_path: str | None = None
) -> str:
    """Clear automation from a clip.
    
    Args:
        track_index: Track containing the clip
        clip_slot: Clip slot index
        parameter_path: Specific parameter to clear (None = all automation)
    
    Returns:
        Confirmation message
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("clear_clip_automation", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "parameter_path": parameter_path,
        })
        return json.dumps({
            "status": "success",
            "message": f"Cleared automation{f' for {parameter_path}' if parameter_path else ''}",
        }, indent=2)
    except Exception as e:
        logger.error(f"Error clearing automation: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def create_automation_envelope(
    ctx: Context,
    track_index: int,
    clip_slot: int,
    parameter_path: str,
    envelope_type: str = "linear",
    cycles: int = 1
) -> str:
    """Create a predefined automation envelope shape.
    
    Generates common automation patterns without manual breakpoints.
    
    Args:
        track_index: Track containing the clip
        clip_slot: Clip slot index
        parameter_path: Device parameter path
        envelope_type: Shape type:
            - "linear": Ramp from 0 to 1
            - "curve_up": Exponential rise
            - "curve_down": Exponential fall
            - "triangle": Up then down
            - "sine": Smooth oscillation
            - "square": On/off steps
        cycles: Number of cycles within clip (default: 1)
    
    Returns:
        Confirmation with generated envelope info
    """
    try:
        ableton = get_ableton(ctx)
        
        # Get clip length first
        clip_info = await ableton.send_command("get_clip_info", {
            "track_index": track_index,
            "clip_slot": clip_slot,
        })
        
        clip_length = clip_info.get("length", 4.0)
        cycle_length = clip_length / cycles
        
        # Generate breakpoints based on envelope type
        breakpoints = []
        
        if envelope_type == "linear":
            for c in range(cycles):
                offset = c * cycle_length
                breakpoints.extend([
                    {"time": offset, "value": 0.0},
                    {"time": offset + cycle_length, "value": 1.0},
                ])
        
        elif envelope_type == "triangle":
            for c in range(cycles):
                offset = c * cycle_length
                mid = offset + cycle_length / 2
                breakpoints.extend([
                    {"time": offset, "value": 0.0},
                    {"time": mid, "value": 1.0},
                    {"time": offset + cycle_length, "value": 0.0},
                ])
        
        elif envelope_type == "square":
            for c in range(cycles):
                offset = c * cycle_length
                mid = offset + cycle_length / 2
                breakpoints.extend([
                    {"time": offset, "value": 0.0},
                    {"time": mid - 0.01, "value": 0.0},
                    {"time": mid, "value": 1.0},
                    {"time": offset + cycle_length - 0.01, "value": 1.0},
                ])
        
        else:  # sine, curve patterns
            import math
            points_per_cycle = 16
            for c in range(cycles):
                for i in range(points_per_cycle + 1):
                    t = c * cycle_length + (i / points_per_cycle) * cycle_length
                    if envelope_type == "sine":
                        value = (math.sin(2 * math.pi * i / points_per_cycle) + 1) / 2
                    elif envelope_type == "curve_up":
                        value = (i / points_per_cycle) ** 2
                    else:  # curve_down
                        value = 1 - (i / points_per_cycle) ** 2
                    breakpoints.append({"time": t, "value": value})
        
        # Set the automation
        result = await ableton.send_command("set_clip_automation", {
            "track_index": track_index,
            "clip_slot": clip_slot,
            "parameter_path": parameter_path,
            "breakpoints": breakpoints,
        })
        
        return json.dumps({
            "status": "success",
            "envelope_type": envelope_type,
            "cycles": cycles,
            "breakpoints_created": len(breakpoints),
        }, indent=2)
    except Exception as e:
        logger.error(f"Error creating envelope: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Browser Tools
# =============================================================================

@mcp.tool()
async def get_browser_tree(
    ctx: Context,
    category_type: str = "all"
) -> str:
    """Get a hierarchical tree of browser categories from Ableton.
    
    Parameters:
    - category_type: Type of categories to get ('all', 'instruments', 'sounds', 'drums', 'audio_effects', 'midi_effects')
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_browser_tree", {
            "category_type": category_type
        })
        
        total_categories = len(result.get("categories", []))
        formatted_output = f"Browser tree for '{category_type}' ({total_categories} categories):\n\n"
        
        def format_tree(item, indent=0):
            output = ""
            if item:
                prefix = "  " * indent
                name = item.get("name", "Unknown")
                uri = item.get("uri", "")
                is_loadable = item.get("is_loadable", False)
                
                output += f"{prefix}• {name}"
                if is_loadable:
                    output += " [loadable]"
                if uri:
                    output += f" (uri: {uri})"
                output += "\n"
                
                for child in item.get("children", []):
                    output += format_tree(child, indent + 1)
            return output
        
        for category in result.get("categories", []):
            formatted_output += format_tree(category)
            formatted_output += "\n"
        
        return formatted_output
    except Exception as e:
        logger.error(f"Error getting browser tree: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def get_browser_items_at_path(ctx: Context, path: str) -> str:
    """Get browser items at a specific path in Ableton's browser.
    
    Parameters:
    - path: Path in the format "category/folder/subfolder"
            where category is one of: instruments, sounds, drums, audio_effects, midi_effects
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("get_browser_items_at_path", {
            "path": path
        })
        
        if "error" in result:
            return json.dumps({
                "error": result["error"],
                "available_categories": result.get("available_categories", [])
            }, indent=2)
        
        return json.dumps(result, indent=2)
    except Exception as e:
        logger.error(f"Error getting browser items: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def load_instrument_or_effect(
    ctx: Context,
    track_index: int,
    uri: str
) -> str:
    """Load an instrument or effect onto a track using its URI.
    
    Parameters:
    - track_index: The index of the track to load the instrument on
    - uri: The URI of the instrument or effect to load (obtained from get_browser_tree or get_browser_items_at_path)
    """
    try:
        ableton = get_ableton(ctx)
        result = await ableton.send_command("load_browser_item", {
            "track_index": track_index,
            "item_uri": uri
        })
        
        if result.get("loaded", False):
            new_devices = result.get("new_devices", [])
            item_name = result.get("item_name", "Unknown")
            if new_devices:
                return f"Loaded '{item_name}' on track {track_index}. New devices: {', '.join(new_devices)}"
            else:
                devices = result.get("devices_after", [])
                return f"Loaded '{item_name}' on track {track_index}. Devices on track: {', '.join(devices)}"
        else:
            return f"Failed to load instrument with URI '{uri}'"
    except Exception as e:
        logger.error(f"Error loading instrument: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def load_drum_kit(
    ctx: Context,
    track_index: int,
    rack_uri: str,
    kit_path: str
) -> str:
    """Load a drum rack and then load a specific drum kit into it.
    
    Parameters:
    - track_index: The index of the track to load on
    - rack_uri: The URI of the drum rack to load
    - kit_path: Path to the drum kit inside the browser (e.g., 'drums/acoustic/kit1')
    """
    try:
        ableton = get_ableton(ctx)
        
        # Step 1: Load the drum rack
        result = await ableton.send_command("load_browser_item", {
            "track_index": track_index,
            "item_uri": rack_uri
        })
        
        if not result.get("loaded", False):
            return f"Failed to load drum rack with URI '{rack_uri}'"
        
        # Step 2: Get the drum kit items at the specified path
        kit_result = await ableton.send_command("get_browser_items_at_path", {
            "path": kit_path
        })
        
        if "error" in kit_result:
            return f"Loaded drum rack but failed to find drum kit: {kit_result.get('error')}"
        
        # Step 3: Find a loadable drum kit
        kit_items = kit_result.get("items", [])
        loadable_kits = [item for item in kit_items if item.get("is_loadable", False)]
        
        if not loadable_kits:
            return f"Loaded drum rack but no loadable drum kits found at '{kit_path}'"
        
        # Step 4: Load the first loadable kit
        kit_uri = loadable_kits[0].get("uri")
        load_result = await ableton.send_command("load_browser_item", {
            "track_index": track_index,
            "item_uri": kit_uri
        })
        
        return f"Loaded drum rack and kit '{loadable_kits[0].get('name')}' on track {track_index}"
    except Exception as e:
        logger.error(f"Error loading drum kit: {e}")
        return json.dumps({"error": str(e)})


@mcp.tool()
async def discover_plugin_presets(
    ctx: Context,
    manufacturer_filter: str | None = None,
    max_depth: int = 4
) -> str:
    """Discover VST/AU/NKS plugin presets organized by manufacturer.
    
    Scans Ableton's browser to find all available plugin presets,
    including NKS-compatible instruments from Native Instruments (Kontakt, Massive),
    Spectrasonics (Omnisphere, Keyscape), Arturia, and other manufacturers.
    
    Args:
        manufacturer_filter: Optional filter to show only matching manufacturers
                            (e.g., "Native" for Native Instruments, "Spectra" for Spectrasonics)
        max_depth: How deep to scan the browser tree (default: 4)
    
    Returns:
        Summary of discovered plugins organized by manufacturer, including:
        - Total manufacturers, plugins, and presets found
        - NKS-compatible manufacturer count
        - For each manufacturer: plugins list and presets with URIs
    
    Examples:
        discover_plugin_presets()  # List all plugins
        discover_plugin_presets(manufacturer_filter="Native")  # Only Native Instruments
        discover_plugin_presets(manufacturer_filter="Kontakt")  # Kontakt instruments
        discover_plugin_presets(manufacturer_filter="Spectra")  # Spectrasonics
    """
    try:
        ableton = get_ableton(ctx)
        params = {"max_depth": max_depth}
        if manufacturer_filter:
            params["manufacturer_filter"] = manufacturer_filter
        
        result = await ableton.send_command("discover_plugin_presets", params)
        
        if "error" in result:
            return json.dumps(result, indent=2)
        
        # Format output for readability
        summary = result.get("summary", {})
        manufacturers = result.get("manufacturers", [])
        
        output = "# Plugin Preset Discovery\n\n"
        output += "## Summary\n"
        output += f"- **Manufacturers**: {summary.get('total_manufacturers', 0)}\n"
        output += f"- **Total Plugins**: {summary.get('total_plugins', 0)}\n"
        output += f"- **Total Presets**: {summary.get('total_presets', 0)}\n"
        output += f"- **NKS-Compatible**: {summary.get('nks_compatible_manufacturers', 0)}\n"
        
        if summary.get("filter_applied"):
            output += f"- **Filter**: '{summary['filter_applied']}'\n"
        
        output += "\n## Manufacturers\n\n"
        
        for mfr in manufacturers[:20]:  # Limit to first 20 for readability
            name = mfr.get("name", "Unknown")
            nks_badge = " [NKS]" if mfr.get("is_nks_likely") else ""
            preset_count = mfr.get("preset_count", 0)
            plugins = mfr.get("plugins", [])
            presets = mfr.get("presets", [])[:10]  # Limit presets shown
            
            output += f"### {name}{nks_badge}\n"
            output += f"Plugins: {len(plugins)} | Presets: {preset_count}\n\n"
            
            if plugins:
                output += "**Plugins:**\n"
                for p in plugins[:5]:
                    output += f"- {p['name']}\n"
                    output += f"  URI: `{p['uri']}`\n"
                if len(plugins) > 5:
                    output += f"- ... and {len(plugins) - 5} more\n"
                output += "\n"
            
            if presets:
                output += "**Presets:**\n"
                for p in presets:
                    output += f"- {p['name']}\n"
                    output += f"  URI: `{p['uri']}`\n"
                if preset_count > 10:
                    output += f"- ... and {preset_count - 10} more presets\n"
                output += "\n"
        
        if len(manufacturers) > 20:
            output += f"\n... and {len(manufacturers) - 20} more manufacturers\n"
        
        output += "\n---\n"
        output += "\n## Usage\n"
        output += "To load a plugin or preset, use:\n"
        output += "```python\n"
        output += "load_instrument_or_effect(track_index=0, uri=\"<uri from above>\")\n"
        output += "```\n"
        
        return output
    except Exception as e:
        logger.error(f"Error discovering plugins: {e}")
        return json.dumps({"error": str(e)})


# =============================================================================
# Entry Point
# =============================================================================

def main():
    """Run the MCP server."""
    import asyncio
    asyncio.run(mcp.run())


if __name__ == "__main__":
    main()
