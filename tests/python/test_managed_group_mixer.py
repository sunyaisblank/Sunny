"""Exact native Group grant at the static Mixer snapshot seam; no host claim."""

from __future__ import annotations

from typing import Any

import pytest
from live_model import Track
from Sunny.managed_routing import ManagedRouting
from test_managed_mixer import preview_request
from test_managed_mixer import state as mixer_state
from test_managed_routing import apply, group_request


@pytest.fixture
def state(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Retain the real adopted Clip, then model actual read-only Group handles."""
    current = mixer_state.__wrapped__(monkeypatch)
    current.song.master_track.canonical_parent = current.song
    for track in current.song.return_tracks:
        while len(track.mixer_device.sends) < len(current.song.return_tracks):
            track._mixer._add_send("Existing return send")
    group = Track(current.song, "Current strings", "group")
    group.canonical_parent = current.song
    group._mixer._add_send("Existing return send")
    current.song._tracks.append(group)
    current.track._group_actual = group
    monkeypatch.setattr(
        Track, "is_foldable", property(lambda track: track._kind == "group"), raising=False
    )
    monkeypatch.setattr(
        Track,
        "is_grouped",
        property(lambda track: getattr(track, "_group_actual", None) is not None),
    )
    monkeypatch.setattr(
        Track, "group_track", property(lambda track: getattr(track, "_group_actual", None))
    )
    current.song.loop_start, current.song.loop_length = 0.0, 8.0
    current.helper = ManagedRouting(current.registry)
    current.registry._routing = current.helper
    current.group = group
    return current


def test_grouped_mixer_snapshot_without_current_group_approval_declines(state: Any) -> None:
    """Grouping/name/index alone is insufficient, before any native formatter/setter."""
    with pytest.raises(RuntimeError, match="exact current Group grant"):
        state.registry._mixer._snapshot(state.record, preview_request(state, {"mute": True}))
    assert state.volume.writes == [] and state.record.get("_managed_mixer") is None


def test_current_exact_group_permits_only_snapshot_and_grants_no_mixer_role(state: Any) -> None:
    """Source Group-only approval binds handles; separate Clip/Mixer approval is still required."""
    result, operation = apply(state, state.helper.preview_group(group_request()))
    assert set(result) == {"group_adoption"}
    assert result["group_adoption"]["part_authority_granted"] is False
    assert result["group_adoption"]["device_authority_granted"] is False
    assert operation["native_mutation_started"] is False
    before, _ = state.registry._mixer._snapshot(
        state.record, preview_request(state, {"mute": True})
    )
    assert before["track_context"]["is_grouped"] is True
    assert before["binding_observation"]["content_boundary_complete"] is False
    assert before["binding_observation"]["structural_boundary_complete"] is False
    assert state.registry._bindings[("project_a", "part_a")] is state.record
    assert state.volume.writes == [] and state.record.get("_managed_mixer") is None


def test_same_named_native_group_replacement_revokes_mixer_snapshot(state: Any) -> None:
    """An equal-valued new Group cannot inherit the approved native object identity."""
    apply(state, state.helper.preview_group(group_request()))
    replacement = Track(state.song, state.group.name, "group")
    replacement.canonical_parent = state.song
    replacement._mixer._add_send("Existing return send")
    state.song._tracks[1] = replacement
    state.track._group_actual = replacement
    assert not state.helper.verify_group_selection("project_a", "part_a", state.track)
    with pytest.raises(RuntimeError, match="exact current Group grant"):
        state.registry._mixer._snapshot(state.record, preview_request(state, {"mute": True}))
    assert state.volume.writes == [] and state.record.get("_managed_mixer") is None
