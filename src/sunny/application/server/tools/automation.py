"""Automation Tools.

Component: SVAU001A
Domain: SV (Server) | Category: AU (Tools-Automation)

Tools for automation envelope control:
- Get clip automation
- Set clip automation breakpoints
- Clear automation
- Create predefined envelope shapes
"""

from __future__ import annotations

import json
import logging
import math

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.automation")


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
