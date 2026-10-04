"""Finite same-Clip extent/meter revision through actual native properties.

Public Max LOM12.4.5 and pinned Python source are separate candidate evidence.
Unknown MPE/Follow/envelope populations and saved/reopened behavior are not
verified. End-marker assignment never crops, removes or recreates native notes.
"""

from __future__ import annotations

import copy
from typing import Any

from .managed_capacity import note_array_bound, require_response_capacity

GEOMETRY_METHOD = "sunny_managed_update_clip_geometry"
GEOMETRY_PROPERTIES = ("end_marker", "signature_numerator", "signature_denominator")


def valid_geometry(value: Any) -> bool:
    from .managed import _finite_number

    return (
        type(value) is dict
        and set(value) == set(GEOMETRY_PROPERTIES)
        and _finite_number(value["end_marker"])
        and value["end_marker"] > 0.0
        and type(value["signature_numerator"]) is int
        and 1 <= value["signature_numerator"] <= 99
        and type(value["signature_denominator"]) is int
        and value["signature_denominator"] in (1, 2, 4, 8, 16)
    )


def valid_managed_geometry_request(name: str, args: Any) -> bool:
    from .managed import _fingerprint, _key

    if name != GEOMETRY_METHOD or type(args) is not list or len(args) != 1:
        return False
    value = args[0]
    return (
        type(value) is dict
        and set(value)
        == {
            "document_token",
            "operation_id",
            "project_key",
            "binding_key",
            "expected_content_fingerprint",
            "geometry",
        }
        and all(
            _key(value[k]) for k in ("document_token", "operation_id", "project_key", "binding_key")
        )
        and _fingerprint(value["expected_content_fingerprint"])
        and valid_geometry(value["geometry"])
    )


def geometry_note_admission(identity: dict[str, Any], geometry: dict[str, Any]) -> None:
    from .managed import _finite_number

    if identity["entire_clip_population_observed"] is not True:
        raise RuntimeError("ClipGeometryUnavailable: complete native note population is required")
    end = float(geometry["end_marker"])
    for note in identity["notes"]:
        start, duration = note["start_time"], note["duration"]
        if (
            not _finite_number(start)
            or not _finite_number(duration)
            or start < 0.0
            or duration <= 0.0
            or not _finite_number(start + duration)
            or start + duration > end
        ):
            raise RuntimeError(
                "ClipGeometryUnavailable: a native note endpoint exceeds desired end"
            )


def geometry_flags(
    before: dict[str, Any], after: dict[str, Any], geometry: dict[str, Any], end_submitted: bool
) -> dict[str, Any]:
    from .managed import _canonical_bytes, _digest

    old, actual = before["manifest"], after["manifest"]
    old_loop, new_loop = old["clip"]["loop_end"], actual["clip"]["loop_end"]
    if _digest(old_loop) == _digest(new_loop):
        relationship = "unchanged_inactive_boundary"
    elif end_submitted and new_loop == float(geometry["end_marker"]):
        relationship = "followed_end_marker"
    else:
        relationship = "unexpected_change"
    protected = copy.deepcopy(actual)
    for name in GEOMETRY_PROPERTIES:
        protected["clip"][name] = old["clip"][name]
    if relationship != "unexpected_change":
        protected["clip"]["loop_end"] = old_loop
    # The separate note comparisons below locate value vs identity changes.
    protected["notes"] = old["notes"]
    old_ids = {n["note_id"] for n in before["note_identity"]["notes"]}
    new_ids = {n["note_id"] for n in after["note_identity"]["notes"]}
    return {
        "observed_geometry_matches_request": all(
            actual["clip"][k] == geometry[k] for k in GEOMETRY_PROPERTIES
        ),
        "note_values_preserved": _canonical_bytes(after["note_identity"])
        == _canonical_bytes(before["note_identity"]),
        "note_ids_preserved": old_ids == new_ids,
        "note_cardinality_preserved": len(after["note_identity"]["notes"])
        == len(before["note_identity"]["notes"]),
        "other_finite_properties_preserved": _digest(protected) == _digest(old),
        "device_identity_preserved": after.get("device_identity_fingerprint")
        == before.get("device_identity_fingerprint"),
        "loop_end_relationship": relationship,
    }


def guard_geometry_response_capacity(operation: dict[str, Any], before: dict[str, Any]) -> None:
    """Include complete before/after evidence and the reportable partial path."""
    geometry = operation["request"]["geometry"]
    after = {
        k: v
        for k, v in before.items()
        if k
        not in (
            "note_update",
            "note_population_update",
            "clip_geometry_update",
            "adoption",
            "device_update",
            "acknowledgement",
        )
    }
    after["manifest"] = {**before["manifest"], "notes": []}
    after["note_identity"] = {**before["note_identity"], "notes": []}
    after["observed_notes_match_request"] = False
    after["observed_clip_properties_match_request"] = False
    count = len(before["note_identity"]["notes"])
    overrides = {
        id(after["manifest"]["notes"]): note_array_bound(count, False),
        id(after["note_identity"]["notes"]): note_array_bound(count, True),
    }
    supplement = {
        "before_manifest": before["manifest"],
        "before_note_identity": before["note_identity"],
        "before_note_identity_fingerprint": before["note_identity_fingerprint"],
        "before_device_identity_fingerprint": before.get("device_identity_fingerprint"),
        "requested_geometry": geometry,
        "submitted_properties": list(GEOMETRY_PROPERTIES),
        "returned_properties": list(GEOMETRY_PROPERTIES),
        **dict.fromkeys(
            (
                "observed_geometry_matches_request",
                "note_values_preserved",
                "note_ids_preserved",
                "note_cardinality_preserved",
                "other_finite_properties_preserved",
                "device_identity_preserved",
            ),
            False,
        ),
        "loop_end_relationship": "unchanged_inactive_boundary",
    }
    progress = {
        "started_properties": list(GEOMETRY_PROPERTIES),
        "returned_properties": list(GEOMETRY_PROPERTIES),
    }
    acknowledged = {
        **operation,
        "outcome": "acknowledged",
        "progress": progress,
        "result": {**after, "clip_geometry_update": supplement},
    }
    require_response_capacity({"success": True, "value": acknowledged}, overrides)
    partial = {
        **operation,
        "outcome": "indeterminate",
        "error": "\U0010ffff" * 1024,
        "partial_binding_retained": False,
        "progress": {**progress, "last_observation": after},
    }
    require_response_capacity({"success": True, "value": partial}, overrides)


class ManagedGeometry:
    def __init__(self, registry: Any) -> None:
        self._registry = registry

    def apply(
        self, record: dict[str, Any], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        registry = self._registry
        if record.get(
            "authority_origin"
        ) == "explicit_adoption" and "clip_geometry_updates" not in record.get(
            "allowed_domains", ()
        ):
            raise RuntimeError(
                "ClipGeometryAuthorityUnavailable: explicit Clip geometry approval is required"
            )
        before = registry._require_in_place_guard(record, request["expected_content_fingerprint"])
        if not {n["note_id"] for n in before["note_identity"]["notes"]} <= record.get(
            "owned_note_ids", set()
        ):
            raise RuntimeError(
                "ClipGeometryAuthorityUnavailable: native note IDs are not all owned"
            )
        geometry = request["geometry"]
        geometry_note_admission(before["note_identity"], geometry)
        properties = [
            k for k in GEOMETRY_PROPERTIES if before["manifest"]["clip"][k] != geometry[k]
        ]
        if not properties:
            raise RuntimeError(
                "GeometryAlreadyMatches: fresh read-only observation already matches"
            )
        guard_geometry_response_capacity(operation, before)
        progress: dict[str, Any] = {"started_properties": [], "returned_properties": []}
        operation["progress"] = progress
        actual = before
        expected = {k: before["manifest"]["clip"][k] for k in GEOMETRY_PROPERTIES}
        for name in properties:
            progress["started_properties"].append(name)
            operation["native_mutation_started"] = True
            try:
                setattr(
                    record["clip"],
                    name,
                    float(geometry[name]) if name == "end_marker" else geometry[name],
                )
            except Exception:
                # Preserve actual finite evidence when the native setter raises
                # after effect; no failure path resumes or compensates.
                try:
                    progress["last_observation"] = registry._capture(record)
                except Exception:
                    pass
                raise
            progress["returned_properties"].append(name)
            expected[name] = geometry[name]
            actual = registry._capture(record)
            progress["last_observation"] = actual
            flags = geometry_flags(
                before, actual, expected, "end_marker" in progress["started_properties"]
            )
            registry._devices.verify_retained_chain(record)
            if not all(flags[k] for k in flags if k != "loop_end_relationship"):
                raise RuntimeError("ClipGeometryPhaseMismatch: stopped later native setters")
        del progress["last_observation"]
        result: dict[str, Any] = registry._seal(record)
        flags = geometry_flags(before, result, geometry, "end_marker" in properties)
        result["clip_geometry_update"] = {
            "before_manifest": before["manifest"],
            "before_note_identity": before["note_identity"],
            "before_note_identity_fingerprint": before["note_identity_fingerprint"],
            "before_device_identity_fingerprint": before.get("device_identity_fingerprint"),
            "requested_geometry": geometry,
            "submitted_properties": properties,
            "returned_properties": list(progress["returned_properties"]),
            **flags,
        }
        return result
