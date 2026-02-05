"""Device Tools.

Component: SVTD001A
Domain: SV (Server) | Category: TD (Tools-Device)

Tools for device and parameter management:
- Get device parameters (including VST/AU plugins)
- Set device parameter values
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.device")


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
