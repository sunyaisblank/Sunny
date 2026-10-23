"""Conservative byte bounds for the existing length-framed JSON response.

server.py uses default json.dumps: ASCII escapes and comma/colon spaces. Every
finite binary64 shortest spelling needs at most 24 ASCII bytes (17 significant
places plus sign, decimal point and exponent); 32 reserves additional margin.
This is a wire-space bound, independent of typed SM1 hashing or musical limits.
"""

from __future__ import annotations

import copy
import json
import math
from typing import Any

MAX_MANAGED_RESPONSE_BYTES = 16 * 1024 * 1024


def json_wire_bound(value: Any, array_bounds: dict[int, int] | None = None) -> int:
    """Bound default json.dumps UTF8 bytes without constructing a giant string."""
    overrides = array_bounds or {}

    def bound(item: Any, depth: int) -> int:
        if depth > 32:
            raise ValueError("Managed wire nesting exceeds 32")
        if item is None:
            return 4
        if type(item) is bool:
            return 5
        if isinstance(item, int):
            if not -(1 << 63) <= item < (1 << 64):
                raise ValueError("Managed wire integer outside domain")
            return len(str(int(item)))
        if type(item) is float:
            if not math.isfinite(item):
                raise ValueError("Managed wire float must be finite")
            return 32
        if type(item) is str:
            item.encode("utf-8")  # Reject unpaired surrogate strings.
            return len(json.dumps(item, ensure_ascii=True))
        if type(item) in (list, tuple):
            if id(item) in overrides:
                return overrides[id(item)]
            return 2 + max(0, len(item) - 1) * 2 + sum(bound(c, depth + 1) for c in item)
        if type(item) is dict and all(type(k) is str for k in item):
            return (
                2
                + max(0, len(item) - 1) * 2
                + sum(bound(k, depth + 1) + 2 + bound(c, depth + 1) for k, c in item.items())
            )
        raise ValueError("Managed wire value outside closed JSON domain")

    return bound(value, 0)


def require_response_capacity(value: Any, array_bounds: dict[int, int] | None = None) -> int:
    """Value is the complete framed object, including success/value envelope."""
    size = json_wire_bound(value, array_bounds)
    if size > MAX_MANAGED_RESPONSE_BYTES:
        raise RuntimeError("ReplyCapacityUnavailable: complete managed response exceeds 16 MiB")
    return size


def guard_managed_response_capacity(operation: dict[str, Any], prospective_result: Any) -> int:
    future = {**operation, "outcome": "acknowledged", "result": prospective_result}
    return require_response_capacity({"success": True, "value": future})


def guard_envelope_author_response_capacity(
    operation: dict[str, Any], before: dict[str, Any], domain: dict[str, Any]
) -> int:
    """Bound the complete known initial-lane ACK before envelope creation."""
    from .managed_envelope_revision import MAX_STEP_POINTS

    if len(operation["request"]["lane"]["points"]) > MAX_STEP_POINTS:
        raise RuntimeError("EnvelopeAuthoringUnavailable: lane exceeds 64 native Step-call budget")
    after = {
        key: copy.deepcopy(value)
        for key, value in before.items()
        if key
        in (
            "track_index",
            "slot_index",
            "manifest",
            "content_fingerprint",
            "note_identity",
            "note_identity_fingerprint",
            "track_tag",
            "clip_tag",
            "structural_boundary_complete",
            "content_boundary_complete",
            "unavailable_reasons",
            "device_identity",
            "device_identity_fingerprint",
            "group_authority",
            "group_authority_fingerprint",
        )
    }
    count = len(before["note_identity"]["notes"])
    after["manifest"]["notes"] = []
    after["note_identity"]["notes"] = []
    after["track_index"] = after["slot_index"] = 2147483647
    after["manifest"]["clip"]["has_envelopes"] = True
    after["structural_boundary_complete"] = False
    after["observed_notes_match_request"] = False
    after["observed_clip_properties_match_request"] = False
    after["unavailable_reasons"].append(
        "Complete native envelope breakpoint population is unavailable"
    )
    after["acknowledgement"] = {
        "action": "created",
        "steps_inserted": len(operation["request"]["lane"]["points"]),
        "parameter": domain,
    }
    future = {**operation, "outcome": "acknowledged", "result": after}
    overrides = {
        id(after["manifest"]["notes"]): note_array_bound(count, False),
        id(after["note_identity"]["notes"]): note_array_bound(count, True),
    }
    return require_response_capacity({"success": True, "value": future}, overrides)


def guard_operation_reservation_capacity(operation: dict[str, Any]) -> int:
    """Reject an unreportable echoed request before reserving or mutating anything.

    A bounded error may contain 1024 non-BMP characters (12 escaped bytes each).
    Population progress includes known returned IDs even when a later phase fails.
    This compact top-level error is not a journal-confirmed Declined operation.
    """
    declined = {
        **operation,
        "outcome": "declined",
        "error": "\U0010ffff" * 1024,
        "partial_binding_retained": False,
    }
    if operation["name"] == "sunny_managed_revise_note_population":
        declined["progress"] = {
            "started_calls": ["remove_notes_by_id", "apply_note_modifications", "add_new_notes"],
            "returned_calls": ["remove_notes_by_id", "apply_note_modifications", "add_new_notes"],
            "returned_added_note_ids": [-2147483648] * len(operation["request"]["additions"]),
        }
    return require_response_capacity({"success": True, "value": declined})


def note_array_bound(count: int, with_ids: bool) -> int:
    """All native scalar widths, not the current requested note's spelling."""
    fields = {
        "pitch": 127,
        "start_time": 0.0,
        "duration": 0.0,
        "velocity": 0.0,
        "mute": False,
        "probability": 0.0,
        "velocity_deviation": 0.0,
        "release_velocity": 0.0,
    }
    if with_ids:
        fields["note_id"] = -2147483648
    return 2 + max(0, count - 1) * 2 + count * json_wire_bound(fields)


def guard_note_response_capacity(
    operation: dict[str, Any],
    before: dict[str, Any],
    after_count: int,
    population: bool = False,
) -> int:
    """Budget immutable before evidence plus worst prospective native population.

    Metadata remains unchanged in this note-only operation; all finite numeric
    fields are still bounded at maximum wire width. Optional separately hashed
    device evidence is included. Old operation supplements are not recaptured.
    """
    after = {
        k: v
        for k, v in before.items()
        if k
        not in (
            "note_update",
            "note_population_update",
            "acknowledgement",
            "device_update",
            "adoption",
        )
    }
    after["manifest"] = {**before["manifest"], "notes": []}
    after["note_identity"] = {**before["note_identity"], "notes": []}
    after["observed_notes_match_request"] = False
    after["observed_clip_properties_match_request"] = False
    request = operation["request"]
    supplement: dict[str, Any] = {
        "before_manifest": before["manifest"],
        "before_note_identity": before["note_identity"],
        "before_note_identity_fingerprint": before["note_identity_fingerprint"],
    }
    overrides = {
        id(after["manifest"]["notes"]): note_array_bound(after_count, False),
        id(after["note_identity"]["notes"]): note_array_bound(after_count, True),
    }
    if population:
        count = len(request["additions"])
        supplement.update(
            {
                "changes_submitted": len(request["changes"]),
                "deletions_submitted": len(request["deletions"]),
                "additions_submitted": count,
                "returned_added_note_ids": [-2147483648] * count,
                "addition_associations": [
                    {"note_key": addition["note_key"], "note_id": -2147483648}
                    for addition in request["additions"]
                ],
                **dict.fromkeys(
                    (
                        "observed_changes_match_request",
                        "observed_deletions_absent",
                        "observed_additions_match_request",
                        "untouched_notes_preserved",
                        "retained_note_ids_preserved",
                        "observed_population_cardinality_match",
                    ),
                    False,
                ),
            }
        )
        after["note_population_update"] = supplement
    else:
        supplement.update(
            {
                "notes_submitted": len(request["changes"]),
                "observed_updates_match_request": False,
                "untouched_notes_preserved": False,
                "note_ids_preserved": False,
            }
        )
        after["note_update"] = supplement
    future = {**operation, "outcome": "acknowledged", "result": after}
    if population:
        future["progress"] = {
            "started_calls": ["remove_notes_by_id", "apply_note_modifications", "add_new_notes"],
            "returned_calls": ["remove_notes_by_id", "apply_note_modifications", "add_new_notes"],
            "returned_added_note_ids": [-2147483648] * len(request["additions"]),
        }
    return require_response_capacity({"success": True, "value": future}, overrides)


def guard_creation_response_capacity(
    operation: dict[str, Any], before: dict[str, Any] | None = None
) -> int:
    """Known note bytes before Track creation; measured metadata before insertion.

    Passing the first, metadata-free necessary check is only candidate admission:
    native Track/Clip metadata is unavailable until creation. The second check
    includes that actual finite metadata and runs before inserting any notes.
    """
    result = dict(before) if before is not None else {}
    result["manifest"] = {**result.get("manifest", {}), "notes": []}
    result["note_identity"] = {**result.get("note_identity", {}), "notes": []}
    result["observed_notes_match_request"] = False
    result["observed_clip_properties_match_request"] = False
    count = len(operation["request"]["notes"])
    if before is not None:
        count += len(before["manifest"]["notes"])
    overrides = {
        id(result["manifest"]["notes"]): note_array_bound(count, False),
        id(result["note_identity"]["notes"]): note_array_bound(count, True),
    }
    future = {**operation, "outcome": "acknowledged", "result": result}
    return require_response_capacity({"success": True, "value": future}, overrides)
