"""Read-only retained-binding volume preview prototype; no authority or setters."""

from __future__ import annotations

from typing import Any

from .native_mixer_units import resolve_native_mixer_display_value
from .native_units import _same


def preview_retained_mixer_volume(
    registry: Any,
    record: dict[str, Any],
    *,
    expected_content_fingerprint: str,
    target: float,
    tolerance: float,
) -> dict[str, Any]:
    """Join the existing private Clip/Device guards to actual Mixer candidate proof.

    The owning operation still supplies epoch/profile checks, finite frame bounds
    and explicit review/fence authority. This prototype creates no preview token,
    modifies no record, refreshes no baseline, and grants no static mixer writes.
    """
    if not any(member is record for member in registry._bindings.values()):
        raise RuntimeError("Mixer preview requires this Registry's retained binding")
    before = registry._require_in_place_guard(record, expected_content_fingerprint)
    devices = registry._devices.private_cohort(record)
    track, clip = record["track"], record["clip"]
    mixer = track.mixer_device
    candidate = resolve_native_mixer_display_value(
        track, mixer=mixer, parameter_kind="volume", target=target, tolerance=tolerance
    )
    current = registry._devices.private_cohort(record)
    if (
        len(devices) != len(current)
        or any(not _same(first, second) for first, second in zip(devices, current))
        or not _same(record["track"], track)
        or not _same(record["clip"], clip)
        or registry._capture(record) != before
    ):
        raise RuntimeError("Mixer preview changed retained native Clip/mixer/device context")
    return {
        "schema_version": 1,
        "binding_observation": before,
        "candidate": candidate,
        "native_mutation_started": False,
        "static_mixer_authority_granted": False,
        "unavailable_fields": [
            "selected_volume_envelope_write_admission",
            "historical_native_identity",
        ],
    }
