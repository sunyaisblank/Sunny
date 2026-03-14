"""
LOM request handler — translates JSON requests to Ableton API calls.

Each request has a type (get/set/call/observe/unobserve), a path into
the LOM object hierarchy, and a property or method name with arguments.

Path navigation:
    "song"                          → Live.Application.get_application().get_document()
    "song/tracks/0"                 → song.tracks[0]
    "song/tracks/0/clip_slots/1"    → song.tracks[0].clip_slots[1]
    "song/tracks/0/devices/0"       → song.tracks[0].devices[0]
    "song/master_track"             → song.master_track
    "song/return_tracks/0"          → song.return_tracks[0]
"""

from __future__ import annotations

import logging
from typing import Any

logger = logging.getLogger("SunnyRemoteScript.handler")


class LomHandler:
    """Translates LomRequest JSON to Ableton LOM API calls."""

    def __init__(self, surface):
        self._surface = surface

    def handle(self, request: dict) -> dict:
        """Dispatch a single request and return a response dict."""
        req_type = request.get("type", "")
        path = request.get("path", "")
        name = request.get("name", "")
        args = request.get("args", [])

        try:
            obj = self._resolve_path(path)

            if req_type == "get":
                value = getattr(obj, name, None)
                if callable(value):
                    value = value()
                return {"success": True, "value": self._serialise(value)}

            elif req_type == "set":
                if args:
                    setattr(obj, name, args[0])
                return {"success": True}

            elif req_type == "call":
                method = getattr(obj, name, None)
                if method is None:
                    return {"success": False, "error": f"No method '{name}' on {path}"}
                result = method(*args)
                return {"success": True, "value": self._serialise(result)}

            else:
                return {"success": False, "error": f"Unknown request type: {req_type}"}

        except AttributeError as e:
            return {"success": False, "error": f"Attribute error: {e}"}
        except IndexError as e:
            return {"success": False, "error": f"Index error: {e}"}
        except Exception as e:
            logger.error("Handler error: %s", e, exc_info=True)
            return {"success": False, "error": str(e)}

    def _resolve_path(self, path: str) -> Any:
        """Navigate the LOM hierarchy from a slash-separated path.

        Starting object is the Song (Live Set document).
        """
        song = self._get_song()
        if not path or path == "song":
            return song

        segments = [s for s in path.split("/") if s]
        if segments[0] == "song":
            segments = segments[1:]

        obj = song
        for segment in segments:
            # Numeric index into a list property
            if segment.isdigit():
                idx = int(segment)
                if hasattr(obj, "__getitem__"):
                    obj = obj[idx]
                else:
                    # Try as a tuple/list
                    obj = list(obj)[idx]
            else:
                obj = getattr(obj, segment)

        return obj

    def _get_song(self) -> Any:
        """Get the current Live Set (Song) object."""
        try:
            # Standard Ableton API path
            return self._surface.song()
        except (AttributeError, TypeError):
            pass

        try:
            # _Framework ControlSurface path
            if hasattr(self._surface, "_c_instance"):
                import Live
                app = Live.Application.get_application()
                return app.get_document()
        except Exception:
            pass

        raise RuntimeError("Cannot access Ableton Song object")

    @staticmethod
    def _serialise(value: Any) -> Any:
        """Convert Ableton objects to JSON-safe Python types."""
        if value is None:
            return None
        if isinstance(value, (bool, int, float, str)):
            return value
        if isinstance(value, (list, tuple)):
            return [LomHandler._serialise(v) for v in value]
        # Ableton vector/tuple types
        try:
            return [LomHandler._serialise(v) for v in value]
        except TypeError:
            return str(value)
