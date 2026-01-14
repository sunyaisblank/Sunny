"""Sunny - Device and Plugin Tools.

Component: SRDV001A
Domain: SR (Server) | Category: DV (Device)

Additional MCP tools for device management, parameter control,
and real-time modulation via UDP/OSC.

Bounds:
    - track_index: int ∈ [0, ∞)
    - device_index: int ∈ [0, ∞)
    - param_index: int ∈ [0, ∞)
    - value: float ∈ [0.0, 1.0]

Error Codes:
    - 1200: Device not found
    - 1201: Parameter not found
    - 1202: Invalid parameter value
"""

from __future__ import annotations

import json
import logging
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from mcp.server.fastmcp import Context

logger = logging.getLogger("sunny.devices")


def register_device_tools(mcp: Any, get_ableton, get_snapshots):
    """Register device-related tools with the MCP server.
    
    Args:
        mcp: FastMCP server instance
        get_ableton: Function to get Ableton connection from context
        get_snapshots: Function to get snapshot manager from context
    """
    
    @mcp.tool()
    async def get_device_list(ctx: Context, track_index: int) -> str:
        """Get all devices on a track.
        
        Args:
            track_index: Index of the track
        
        Returns:
            JSON array of devices with names and types
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("get_device_list", {
                "track_index": track_index,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error getting device list: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def get_device_parameters(
        ctx: Context,
        track_index: int,
        device_index: int = 0
    ) -> str:
        """Get all parameters for a device.
        
        Returns parameters with their current values, min/max ranges,
        and whether they are automatable. Values are normalized to 0.0-1.0.
        
        Args:
            track_index: Index of the track
            device_index: Index of the device on the track (default: 0)
        
        Returns:
            JSON with device name and parameter list
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("get_device_parameters", {
                "track_index": track_index,
                "device_index": device_index,
            })
            return json.dumps(result, indent=2)
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
        """Set a device parameter value.
        
        Args:
            track_index: Index of the track
            device_index: Index of the device
            param_index: Index of the parameter
            value: Normalized value (0.0 to 1.0)
        
        Returns:
            Confirmation with new value
        """
        try:
            ableton = get_ableton(ctx)
            
            value = max(0.0, min(1.0, float(value)))
            
            result = await ableton.send_command("set_device_parameter", {
                "track_index": track_index,
                "device_index": device_index,
                "param_index": param_index,
                "value": value,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error setting device parameter: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def set_device_parameters_batch(
        ctx: Context,
        track_index: int,
        device_index: int,
        parameters: list[dict]
    ) -> str:
        """Set multiple device parameters at once.
        
        More efficient than setting parameters one at a time.
        
        Args:
            track_index: Index of the track
            device_index: Index of the device
            parameters: List of {param_index: int, value: float} dicts
        
        Returns:
            Confirmation with number of parameters set
        
        Example:
            set_device_parameters_batch(0, 0, [
                {"param_index": 1, "value": 0.5},
                {"param_index": 3, "value": 0.75},
            ])
        """
        try:
            ableton = get_ableton(ctx)
            
            # Validate and normalize values
            validated_params = []
            for p in parameters:
                validated_params.append({
                    "param_index": p["param_index"],
                    "value": max(0.0, min(1.0, float(p["value"]))),
                })
            
            result = await ableton.send_command("set_device_parameters_batch", {
                "track_index": track_index,
                "device_index": device_index,
                "parameters": validated_params,
            })
            return f"Set {len(validated_params)} parameters on device {device_index}"
        except Exception as e:
            logger.error(f"Error batch setting parameters: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def load_device_by_uri(
        ctx: Context,
        track_index: int,
        uri: str
    ) -> str:
        """Load a device from Ableton's browser by URI.
        
        The URI is the internal path Ableton uses to identify devices.
        Use get_browser_items to discover available URIs.
        
        Args:
            track_index: Track to load device onto
            uri: Device URI (e.g., "Instruments/Wavetable/Wavetable.adv")
        
        Returns:
            Confirmation with loaded device info
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("load_device", {
                "track_index": track_index,
                "uri": uri,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error loading device: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def delete_device(
        ctx: Context,
        track_index: int,
        device_index: int
    ) -> str:
        """Delete a device from a track.
        
        SAFETY: Creates a snapshot before deletion.
        
        Args:
            track_index: Index of the track
            device_index: Index of the device to delete
        
        Returns:
            Confirmation with snapshot ID
        """
        try:
            # Create snapshot before destructive operation
            snapshots = get_snapshots(ctx)
            snapshot_id = await snapshots.create_snapshot(
                f"Before delete device {device_index} from track {track_index}"
            )
            
            ableton = get_ableton(ctx)
            result = await ableton.send_command("delete_device", {
                "track_index": track_index,
                "device_index": device_index,
            })
            
            return json.dumps({
                "status": "success",
                "message": f"Deleted device {device_index} from track {track_index}",
                "snapshot_id": snapshot_id,
            }, indent=2)
        except Exception as e:
            logger.error(f"Error deleting device: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def enable_device(
        ctx: Context,
        track_index: int,
        device_index: int,
        enabled: bool = True
    ) -> str:
        """Enable or disable a device (bypass).
        
        Args:
            track_index: Index of the track
            device_index: Index of the device
            enabled: True to enable, False to bypass (default: True)
        
        Returns:
            Confirmation message
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("set_device_enabled", {
                "track_index": track_index,
                "device_index": device_index,
                "enabled": enabled,
            })
            return f"Device {'enabled' if enabled else 'bypassed'}"
        except Exception as e:
            logger.error(f"Error enabling device: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def get_browser_instruments(ctx: Context) -> str:
        """Get available instruments from Ableton's browser.
        
        Returns a list of instrument categories and items that can
        be loaded onto tracks using their URIs.
        
        Returns:
            JSON with instrument categories and URIs
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("get_browser_instruments")
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error getting browser instruments: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def get_browser_effects(ctx: Context) -> str:
        """Get available audio effects from Ableton's browser.
        
        Returns a list of effect categories and items that can
        be loaded onto tracks using their URIs.
        
        Returns:
            JSON with effect categories and URIs
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("get_browser_effects")
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error getting browser effects: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def modulate_parameter_realtime(
        ctx: Context,
        track_index: int,
        device_index: int,
        param_index: int,
        value: float
    ) -> str:
        """Modulate a parameter in real-time via UDP (low-latency).
        
        This uses UDP/OSC for <5ms latency, suitable for
        live performance and real-time control.
        
        Args:
            track_index: Index of the track
            device_index: Index of the device
            param_index: Index of the parameter
            value: Normalized value (0.0 to 1.0)
        
        Returns:
            Confirmation (note: UDP is fire-and-forget)
        """
        try:
            ableton = get_ableton(ctx)
            
            value = max(0.0, min(1.0, float(value)))
            
            # Use UDP for low-latency modulation
            osc_address = f"/track/{track_index}/device/{device_index}/param/{param_index}"
            ableton.send_realtime(osc_address, value)
            
            return f"Sent realtime modulation: {value}"
        except Exception as e:
            logger.error(f"Error sending realtime modulation: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def create_sidechain(
        ctx: Context,
        source_track: int,
        target_track: int,
        target_device: int = 0
    ) -> str:
        """Set up sidechain routing from one track to another.
        
        Configures the sidechain input of a compressor or other
        sidechain-capable device to receive signal from the source track.
        
        Args:
            source_track: Track index to sidechain from (e.g., kick drum)
            target_track: Track index with the sidechain device
            target_device: Device index on target track (default: 0)
        
        Returns:
            Confirmation of sidechain setup
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("setup_sidechain", {
                "source_track": source_track,
                "target_track": target_track,
                "target_device": target_device,
            })
            return json.dumps({
                "status": "success",
                "message": f"Sidechain configured from track {source_track} to track {target_track}",
            }, indent=2)
        except Exception as e:
            logger.error(f"Error setting up sidechain: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def auto_gain_tracks(
        ctx: Context,
        track_indices: list[int] | None = None,
        target_db: float = -6.0
    ) -> str:
        """Automatically adjust track volumes for proper gain staging.
        
        Analyzes peak levels and adjusts volumes to maintain
        the specified headroom.
        
        Args:
            track_indices: List of track indices to adjust (None = all tracks)
            target_db: Target peak level in dB (default: -6.0)
        
        Returns:
            Report of adjustments made
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("auto_gain_tracks", {
                "track_indices": track_indices,
                "target_db": target_db,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error auto-gaining tracks: {e}")
            return json.dumps({"error": str(e)})


def register_send_tools(mcp: Any, get_ableton):
    """Register send/return track tools.
    
    Args:
        mcp: FastMCP server instance
        get_ableton: Function to get Ableton connection from context
    """
    
    @mcp.tool()
    async def create_return_track(ctx: Context, name: str | None = None) -> str:
        """Create a new return track.
        
        Args:
            name: Optional name for the return track
        
        Returns:
            JSON with created return track info
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("create_return_track", {
                "name": name,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error creating return track: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def set_send_level(
        ctx: Context,
        track_index: int,
        send_index: int,
        level: float
    ) -> str:
        """Set the send level from a track to a return track.
        
        Args:
            track_index: Source track index
            send_index: Send/return index (0 = Send A, 1 = Send B, etc.)
            level: Send level (0.0 to 1.0)
        
        Returns:
            Confirmation with new level
        """
        try:
            ableton = get_ableton(ctx)
            
            level = max(0.0, min(1.0, float(level)))
            
            result = await ableton.send_command("set_send_level", {
                "track_index": track_index,
                "send_index": send_index,
                "level": level,
            })
            return f"Track {track_index} Send {chr(65 + send_index)} set to {int(level * 100)}%"
        except Exception as e:
            logger.error(f"Error setting send level: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def get_send_levels(ctx: Context, track_index: int) -> str:
        """Get all send levels for a track.
        
        Args:
            track_index: Track index to query
        
        Returns:
            JSON with send levels for all return tracks
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("get_send_levels", {
                "track_index": track_index,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error getting send levels: {e}")
            return json.dumps({"error": str(e)})


def register_group_tools(mcp: Any, get_ableton):
    """Register group track tools.
    
    Args:
        mcp: FastMCP server instance
        get_ableton: Function to get Ableton connection from context
    """
    
    @mcp.tool()
    async def create_group_track(
        ctx: Context,
        track_indices: list[int],
        name: str | None = None
    ) -> str:
        """Create a group track containing the specified tracks.
        
        Args:
            track_indices: List of track indices to group
            name: Optional name for the group
        
        Returns:
            JSON with created group info
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("create_group_track", {
                "track_indices": track_indices,
                "name": name,
            })
            return json.dumps(result, indent=2)
        except Exception as e:
            logger.error(f"Error creating group track: {e}")
            return json.dumps({"error": str(e)})
    
    @mcp.tool()
    async def fold_group(
        ctx: Context,
        group_track_index: int,
        folded: bool = True
    ) -> str:
        """Fold or unfold a group track.
        
        Args:
            group_track_index: Index of the group track
            folded: True to fold, False to unfold (default: True)
        
        Returns:
            Confirmation message
        """
        try:
            ableton = get_ableton(ctx)
            result = await ableton.send_command("set_group_folded", {
                "track_index": group_track_index,
                "folded": folded,
            })
            return f"Group {'folded' if folded else 'unfolded'}"
        except Exception as e:
            logger.error(f"Error folding group: {e}")
            return json.dumps({"error": str(e)})
