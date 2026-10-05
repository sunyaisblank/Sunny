"""Explicit current Group approval after an external native regrouping model."""

from __future__ import annotations

import copy
from typing import Any

import pytest
from live_model import Track
from Sunny.managed import _digest
from test_managed_routing import APPLY_METHOD, apply, approved, current_group, group_request
from test_managed_routing import state as routing_state


def test_explicit_group_b_retires_stale_a_without_changing_native_members_or_history(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Current B approval cannot be shadowed by A; A's other member needs fresh approval."""
    state: Any = routing_state.__wrapped__(monkeypatch)
    group_a = current_group(state, monkeypatch)
    state.registry._routing = state.helper
    sibling = Track(state.song, "Sunny|project_a|part_c|track", "midi")
    sibling.canonical_parent = state.song
    while len(sibling.mixer_device.sends) < len(state.song.return_tracks):
        sibling._mixer._add_send("Existing return send")
    sibling._group_actual = group_a
    state.song._tracks.append(sibling)
    original = group_request()
    original["intent"]["member_binding_keys"] = ["part_a", "part_c"]
    a_receipt, a_operation = apply(state, state.helper.preview_group(original))
    old_receipt = copy.deepcopy(a_receipt)
    old_operation = copy.deepcopy(a_operation)
    assert state.helper.verify_group_selection("project_a", "part_a", state.track)
    assert state.helper.verify_group_selection("project_a", "part_c", sibling)
    notes = dict(state.clip._notes)

    group_b = Track(state.song, "Current brass", "group")
    group_b.canonical_parent = state.song
    group_b._mixer._add_send("Existing return send")
    state.song._tracks.append(group_b)
    # External provider topology edit, not a Remote Script grouping setter.
    state.track._group_actual = group_b
    assert not state.helper.verify_group_selection("project_a", "part_a", state.track)
    assert state.helper.capture_group_authority("project_a", "part_a", state.track) == {}
    with pytest.raises(RuntimeError, match="membership changed"):
        state.helper.verify_group_selection("project_a", "part_c", sibling)

    desired = group_request()
    desired["intent"].update(group_key="group_brass", track_index=3)
    preview_b = state.helper.preview_group(desired)
    payload_b = approved(preview_b)
    payload_b["operation_id"] = "route_b"
    b_operation = {
        "document_token": "document_a",
        "operation_id": "route_b",
        "name": APPLY_METHOD,
        "request": payload_b,
        "request_fingerprint": _digest({"name": APPLY_METHOD, "request": payload_b}),
        "outcome": "pending",
        "native_mutation_started": False,
    }
    b_receipt = state.helper.apply(("project_a", "part_a"), payload_b, b_operation)
    assert b_receipt["group_adoption"]["part_authority_granted"] is False
    assert b_receipt["group_adoption"]["device_authority_granted"] is False
    assert b_operation["native_mutation_started"] is False
    assert state.helper.verify_group_selection("project_a", "part_a", state.track)
    assert not state.helper.verify_group_selection("project_a", "part_c", sibling)
    assert ("project_a", "group_strings") not in state.helper._groups
    proof = state.helper.capture_group_authority("project_a", "part_a", state.track)
    assert proof["group_authority"]["group_key"] == "group_brass"
    assert proof["group_authority"]["group_track_index"] == 3
    assert proof["group_authority"]["member_track_indices"] == [0]
    assert state.registry._bindings == {} and state.calls == []
    assert dict(state.clip._notes) == notes and set(notes) == {41, 99}
    assert state.track.group_track is group_b and sibling.group_track is group_a
    assert a_receipt == old_receipt and a_operation == old_operation
    assert a_operation["operation_id"] == "route_a" and b_operation["operation_id"] == "route_b"
