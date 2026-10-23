"""Explicit whole selected-Pan envelope replacement through actual native objects.

Samples are incomplete evidence; approved unsampled selected state can be
replaced. Same-parameter modulation, native envelope loops and persistence are
unqualified. The64-Step budget bounds native main-thread work, not host latency.
"""

from __future__ import annotations

import copy
import uuid
from typing import Any

from .managed_capacity import (
    guard_managed_response_capacity,
    note_array_bound,
    require_response_capacity,
)
from .native_control import check_native_peer, native_call

PREVIEW_METHOD = "sunny_managed_preview_envelope_replacement"
REPLACE_METHOD = "sunny_managed_replace_envelope"
MAX_STEP_POINTS = 64
MAX_PREVIEWS = 256
SCOPE = {
    "replacement_scope": "selected_parameter_envelope",
    "breakpoint_population_observed": False,
    "unsampled_state_preservation_proven": False,
    "other_parameter_envelopes_written": False,
    "historical_envelope_identity_proven": False,
    "same_parameter_modulation_preservation_proven": False,
    "host_qualified": False,
}


def valid_revision_request(name: str, args: Any) -> bool:
    from .handler import _valid_step_envelope_author
    from .managed import _fingerprint, _key

    if type(args) is not list or len(args) != 1 or type(args[0]) is not dict:
        return False
    request = args[0]
    common = {"document_token", "project_key", "binding_key"}
    if not all(_key(request.get(k)) for k in common):
        return False
    if name == PREVIEW_METHOD:
        return (
            set(request) == common | {"expected_content_fingerprint", "lane"}
            and _fingerprint(request["expected_content_fingerprint"])
            and _valid_step_envelope_author(request["lane"])
            and request["lane"]["parameter"] == {"kind": "panning"}
            and len(request["lane"]["points"]) <= MAX_STEP_POINTS
        )
    return (
        name == REPLACE_METHOD
        and set(request)
        == common
        | {
            "operation_id",
            "preview_token",
            "preview_fingerprint",
            "explicit_selected_envelope_replacement",
            "allow_unsampled_selected_state_overwrite",
        }
        and _key(request["operation_id"])
        and type(request["preview_token"]) is str
        and len(request["preview_token"]) == 32
        and all(c in "0123456789abcdef" for c in request["preview_token"])
        and _fingerprint(request["preview_fingerprint"])
        and request["explicit_selected_envelope_replacement"] is True
        and request["allow_unsampled_selected_state_overwrite"] is True
    )


def lane_steps(lane: dict[str, Any]) -> list[tuple[float, float, float, float]]:
    """Require representable positive intervals and strictly interior sample times."""
    from .managed import _finite_number

    result = []
    for i, point in enumerate(lane["points"]):
        start = float(point["time"])
        end = float(
            lane["points"][i + 1]["time"] if i + 1 < len(lane["points"]) else lane["clip_end"]
        )
        duration = end - start
        interior = start + duration / 2.0
        if not all(_finite_number(x) for x in (start, end, duration, interior)) or not (
            start < interior < end and duration > 0.0 and start + duration == end
        ):
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: interval has no exact finite interior"
            )
        result.append((start, duration, float(point["value"]), interior))
    return result


def revision_flags(before: dict[str, Any], after: dict[str, Any]) -> dict[str, bool]:
    from .managed import _canonical_bytes

    protected = copy.deepcopy(after["manifest"])
    protected["clip"]["has_envelopes"] = before["manifest"]["clip"]["has_envelopes"]
    return {
        "note_ids_and_values_preserved": _canonical_bytes(after["note_identity"])
        == _canonical_bytes(before["note_identity"]),
        "other_finite_properties_preserved": _canonical_bytes(protected)
        == _canonical_bytes(before["manifest"]),
        "device_identity_preserved": before.get("device_identity_fingerprint")
        == after.get("device_identity_fingerprint"),
    }


def guard_revision_capacity(operation: dict[str, Any], preview: dict[str, Any]) -> None:
    """Include complete before/after populations and reportable partial-stage evidence."""
    before = preview["observation"]
    after = copy.deepcopy(before)
    after["manifest"]["notes"] = []
    after["note_identity"]["notes"] = []
    count = len(before["note_identity"]["notes"])
    overrides = {
        id(after["manifest"]["notes"]): note_array_bound(count, False),
        id(after["note_identity"]["notes"]): note_array_bound(count, True),
    }
    metadata = {k: v for k, v in preview.items() if k not in ("observation", "preview_fingerprint")}
    calls = ["clear_envelope", "create_automation_envelope"] + ["insert_step"] * len(
        preview["lane"]["points"]
    )
    progress = {"started_calls": calls, "returned_calls": calls}
    result = {
        **after,
        "envelope_replacement": {
            "before_observation": before,
            "preview_metadata": metadata,
            "preview_fingerprint": preview["preview_fingerprint"],
            "actual_samples": [
                {"time": step[3], "value": 0.0} for step in lane_steps(preview["lane"])
            ],
            "observed_step_samples_match_request": False,
            **dict.fromkeys(revision_flags(before, before), False),
        },
    }
    require_response_capacity(
        {
            "success": True,
            "value": {
                **operation,
                "outcome": "acknowledged",
                "progress": progress,
                "result": result,
            },
        },
        overrides,
    )
    require_response_capacity(
        {
            "success": True,
            "value": {
                **operation,
                "outcome": "indeterminate",
                "error": "\U0010ffff" * 1024,
                "partial_binding_retained": False,
                "progress": {**progress, "last_observation": after},
            },
        },
        overrides,
    )


class ManagedEnvelopeRevision:
    def __init__(self, registry: Any) -> None:
        self._registry = registry
        self._previews: dict[str, dict[str, Any]] = {}

    def _target(
        self, record: dict[str, Any], lane: dict[str, Any]
    ) -> tuple[Any, dict[str, Any], list[tuple[float, float, float, float]]]:
        registry = self._registry
        track_index, slot_index = registry._indices(record)
        path = f"song/tracks/{track_index}/clip_slots/{slot_index}/clip"
        _, parameter, domain = registry._handler._step_envelope_target(
            path, record["clip"], {"kind": "panning"}
        )
        domain["state"] = int(domain["state"])
        domain["automation_state"] = int(domain["automation_state"])
        if (
            registry._handler._step_clip_interval(record["clip"], idle=True)
            != float(lane["clip_end"])
            or record["track"].mixer_device.panning_mode != 0
            or domain["original_name"] != "Track Panning"
            or domain["minimum"] != -1.0
            or domain["maximum"] != 1.0
            or domain["automation_state"] == 2
            or any(
                not domain["minimum"] <= float(p["value"]) <= domain["maximum"]
                for p in lane["points"]
            )
        ):
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: current Pan domain or marker differs"
            )
        if any(
            not callable(getattr(record["clip"], name, None))
            for name in ("clear_envelope", "automation_envelope", "create_automation_envelope")
        ):
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: parameter-scoped Python API is absent"
            )
        return parameter, domain, lane_steps(lane)

    def _samples(
        self, parameter: Any, envelope: Any, domain: dict[str, Any], steps: Any
    ) -> dict[str, Any]:
        registry = self._registry
        present = registry._handler._envelope_valid(envelope)
        samples = []
        if present:
            if not callable(getattr(envelope, "value_at_time", None)):
                raise RuntimeError(
                    "EnvelopeReplacementUnavailable: native envelope sampling is absent"
                )
            for _, _, _, time in steps:
                value = registry._handler._lom_float(
                    envelope.value_at_time(time), "replacement sampled value"
                )
                if not domain["minimum"] <= value <= domain["maximum"]:
                    raise RuntimeError(
                        "EnvelopeReplacementUnavailable: native sample exceeds Pan domain"
                    )
                samples.append({"time": time, "value": value})
        return {"has_envelope": present, "parameter": domain, "samples": samples}

    def preview(self, record: dict[str, Any], request: dict[str, Any]) -> dict[str, Any]:
        from .managed import _digest

        registry = self._registry
        if request["document_token"] != registry._document_token:
            raise RuntimeError("EnvelopeReplacementUnavailable: current document epoch differs")
        before = registry._require_in_place_guard(record, request["expected_content_fingerprint"])
        parameter, domain, steps = self._target(record, request["lane"])
        envelope = record["clip"].automation_envelope(parameter)
        if len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("EnvelopeReplacementUnavailable: preview registry is full")
        token = uuid.uuid4().hex
        preview = {
            "schema_version": 1,
            "context": {
                "bridge_instance": registry._bridge_instance,
                "document_token": registry._document_token,
            },
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "preview_token": token,
            "lane": copy.deepcopy(request["lane"]),
            "observation": before,
            "selected_envelope": self._samples(parameter, envelope, domain, steps),
            "scope": dict(SCOPE),
            "native_step_call_budget": MAX_STEP_POINTS,
        }
        preview["preview_fingerprint"] = _digest(preview)
        prospective = {
            "document_token": request["document_token"],
            "operation_id": "a" * 64,
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "preview_token": token,
            "preview_fingerprint": preview["preview_fingerprint"],
            "explicit_selected_envelope_replacement": True,
            "allow_unsampled_selected_state_overwrite": True,
        }
        operation = {
            "document_token": request["document_token"],
            "operation_id": "a" * 64,
            "name": REPLACE_METHOD,
            "request": prospective,
            "request_fingerprint": "f" * 64,
            "outcome": "pending",
            "native_mutation_started": False,
        }
        require_response_capacity({"success": True, "value": preview})
        guard_revision_capacity(operation, preview)
        # Capture again before retaining the private preview; sampling is not a guard refresh.
        final = registry._require_in_place_guard(record, request["expected_content_fingerprint"])
        if final != before:
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: native state changed during preview"
            )
        self._previews[token] = {
            "preview": copy.deepcopy(preview),
            "record": record,
            "track": record["track"],
            "slot": record["slot"],
            "clip": record["clip"],
            "parameter": parameter,
            "envelope": envelope,
        }
        return preview

    def apply(
        self, record: dict[str, Any], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        from .managed import _digest

        registry = self._registry
        retained = self._previews.get(request["preview_token"])
        if retained is None or retained["record"] is not record:
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: exact current preview is not retained"
            )
        preview = retained["preview"]
        if (
            preview["preview_fingerprint"] != request["preview_fingerprint"]
            or preview["context"]
            != {
                "bridge_instance": registry._bridge_instance,
                "document_token": registry._document_token,
            }
            or any(preview[k] != request[k] for k in ("project_key", "binding_key"))
        ):
            raise RuntimeError("EnvelopeReplacementUnavailable: approved preview context differs")
        current = registry._require_in_place_guard(
            record, preview["observation"]["content_fingerprint"]
        )
        before = preview["observation"]
        normalized_current = {
            **current,
            "track_index": before["track_index"],
            "slot_index": before["slot_index"],
        }
        if any(
            not registry._same(record[name], retained[name]) for name in ("track", "slot", "clip")
        ):
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: retained native Track/Slot/Clip identities differ"
            )
        parameter, domain, steps = self._target(record, preview["lane"])
        envelope = record["clip"].automation_envelope(parameter)
        if (
            not registry._same(parameter, retained["parameter"])
            or not registry._same(envelope, retained["envelope"])
            or normalized_current != before
            or self._samples(parameter, envelope, domain, steps) != preview["selected_envelope"]
        ):
            raise RuntimeError(
                "EnvelopeReplacementUnavailable: native handles or sampled state drifted"
            )
        guard_revision_capacity(operation, preview)
        del self._previews[request["preview_token"]]
        progress: dict[str, Any] = {"started_calls": [], "returned_calls": []}
        operation["progress"] = progress

        def verify() -> dict[str, Any]:
            parameter_now, _, _ = self._target(record, preview["lane"])
            if not registry._same(parameter_now, parameter):
                raise RuntimeError("EnvelopeReplacementPhaseMismatch: actual Pan identity changed")
            actual: dict[str, Any] = registry._capture(record)
            progress["last_observation"] = actual
            registry._devices.verify_retained_chain(record)
            if not all(revision_flags(before, actual).values()):
                raise RuntimeError(
                    "EnvelopeReplacementPhaseMismatch: protected native state changed"
                )
            return actual

        def native(name: str, function: Any, *args: Any) -> Any:
            check_native_peer()
            progress["started_calls"].append(name)
            operation["native_mutation_started"] = True
            try:
                result = native_call(function, *args)
            except Exception:
                try:
                    progress["last_observation"] = registry._capture(record)
                except Exception:
                    pass
                raise
            progress["returned_calls"].append(name)
            verify()
            return result

        if registry._handler._envelope_valid(envelope):
            native("clear_envelope", record["clip"].clear_envelope, parameter)
            if registry._handler._envelope_valid(record["clip"].automation_envelope(parameter)):
                raise RuntimeError(
                    "EnvelopeReplacementPhaseMismatch: selected envelope was not cleared"
                )
        envelope = native(
            "create_automation_envelope", record["clip"].create_automation_envelope, parameter
        )
        if (
            not registry._handler._envelope_valid(envelope)
            or not registry._same(envelope, record["clip"].automation_envelope(parameter))
            or not callable(getattr(envelope, "insert_step", None))
        ):
            raise RuntimeError("EnvelopeReplacementPhaseMismatch: actual envelope creation differs")
        for start, duration, value, time in steps:
            if not registry._same(envelope, record["clip"].automation_envelope(parameter)):
                raise RuntimeError(
                    "EnvelopeReplacementPhaseMismatch: selected envelope identity changed"
                )
            native("insert_step", envelope.insert_step, start, duration, value)
            if (
                registry._handler._lom_float(
                    envelope.value_at_time(time), "replacement step readback"
                )
                != value
            ):
                raise RuntimeError(
                    "EnvelopeReplacementPhaseMismatch: independent step sample differs"
                )
        result: dict[str, Any] = registry._seal(record)
        actual_samples = self._samples(parameter, envelope, domain, steps)["samples"]
        result["envelope_replacement"] = {
            "before_observation": before,
            "preview_metadata": {
                k: v for k, v in preview.items() if k not in ("observation", "preview_fingerprint")
            },
            "preview_fingerprint": request["preview_fingerprint"],
            "actual_samples": actual_samples,
            "observed_step_samples_match_request": len(actual_samples) == len(steps)
            and all(
                s["time"] == step[3] and s["value"] == step[2]
                for s, step in zip(actual_samples, steps)
            ),
            **revision_flags(before, result),
        }
        progress.pop("last_observation", None)
        guard_managed_response_capacity(operation, result)
        # Reconstructibility is independent of request echo or authored-point verification.
        if (
            _digest({**result["envelope_replacement"]["preview_metadata"], "observation": before})
            != request["preview_fingerprint"]
        ):
            raise RuntimeError("EnvelopeReplacementPhaseMismatch: preview closure differs")
        return result
