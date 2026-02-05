"""Browser Tools.

Component: SVTB001A
Domain: SV (Server) | Category: TB (Tools-Browser)

Tools for browser navigation and loading:
- Get browser tree
- Get browser items at path
- Load instruments and effects
- Load drum kits
- Discover plugin presets
"""

from __future__ import annotations

import json
import logging

from mcp.server.fastmcp import Context

from sunny.application.server.mcp import mcp
from sunny.application.server.context import get_ableton

logger = logging.getLogger("sunny.tools.browser")


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
