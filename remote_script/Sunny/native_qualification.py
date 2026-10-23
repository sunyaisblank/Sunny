"""Read-only same-Song join for the separately gated native qualification probe.

Registrations last only for the interpreter lifetime. Weak references do not
keep a removed surface alive, and module reloads cannot hide a surviving primary.
"""

from __future__ import annotations

import builtins
import re
import threading
import weakref
from typing import Any

_SLOT = "_sunny_primary_qualification_registrations_v1"


def _state(create: bool = False) -> weakref.WeakSet:
    if not hasattr(builtins, _SLOT) and create:
        setattr(builtins, _SLOT, {"version": 1, "registries": weakref.WeakSet()})
    value = getattr(builtins, _SLOT, None)
    if (
        type(value) is not dict
        or set(value) != {"version", "registries"}
        or type(value["version"]) is not int
        or value["version"] != 1
        or not isinstance(value["registries"], weakref.WeakSet)
    ):
        raise RuntimeError("Primary qualification registration is unavailable")
    return value["registries"]


def register_primary(registry: Any) -> bool:
    """Register an initialized primary without any native getter or persistent setting."""
    try:
        if registry._main_thread != threading.get_ident():
            return False
        _state(create=True).add(registry)
        return True
    except (AttributeError, TypeError, RuntimeError):
        return False


def unregister_primary(registry: Any) -> bool:
    """Withdraw only this primary; malformed external state is never replaced."""
    try:
        _state().discard(registry)
        return True
    except (TypeError, RuntimeError):
        return False


def observe_primary_context(observed_song: Any) -> dict[str, Any]:
    """Join one primary's epoch to the probe's actual Song in the same UI callback."""
    registries = list(_state())
    if len(registries) != 1:
        raise RuntimeError("Exactly one active Sunny primary is required for qualification")
    registry = registries[0]
    if registry._main_thread != threading.get_ident():
        raise RuntimeError("Primary qualification requires the Live main thread")
    try:
        current = registry._ensure_document()
        if observed_song is None or not (current is observed_song or current == observed_song):
            raise RuntimeError(
                "Qualification probe and primary do not observe the same native Song"
            )
        bridge = registry._bridge_instance
        document = registry._document_token
        if any(
            type(value) is not str or re.fullmatch(r"[0-9a-f]{32}", value) is None
            for value in (bridge, document)
        ):
            raise RuntimeError("Primary qualification epoch is unavailable")
        return {"schema_version": 1, "bridge_instance": bridge, "document_token": document}
    except RuntimeError:
        raise
    except Exception as error:
        raise RuntimeError("Primary qualification native identity is unavailable") from error
