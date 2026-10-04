"""Actual MCP/TCP qualification when the native provider changes clip geometry.

The external provider deliberately interprets requested end 4 as end 8. This
literal witness tests truthful readback and later admission, not a Live host.
"""

from __future__ import annotations

import pytest
from live_model import Clip
from test_mcp_realization_workflow import (
    NativeWorkflow,
    _author,
    _literal_notes,
    _ok,
    _realize,
)


@pytest.fixture
def mismatching_native_workflow(tmp_path, monkeypatch):
    """Retain real bridge routing while changing only external native interpretation."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    native_setattr = Clip.__setattr__

    def set_native_value(clip, name, value):
        if name == "end_marker" and value == 4.0:
            value = 8.0
        native_setattr(clip, name, value)

    monkeypatch.setattr(Clip, "__setattr__", set_native_value)
    try:
        yield workflow
    finally:
        workflow.close()


def test_native_mismatched_geometry_retains_ownership_without_qualifying_updates(
    mismatching_native_workflow,
):
    """Known mismatched geometry cannot become success through stable readback or note edits."""
    workflow = mismatching_native_workflow
    with workflow.process() as client:
        _author(client, workflow)
        created = _realize(client, "project_realization_create")
        assert created["success"] is False, created
        assert created["state"] == "verification_mismatch", created
        assert created["native_projection_verified"] is False
        assert created["history_saved"] is True
        assert created["receipt"]["outcome"] == "acknowledged"
        assert created["binding"]["observation"]["observed_notes_match_request"] is True
        assert created["binding"]["observation"]["observed_clip_properties_match_request"] is False
        owned = workflow.live.song.tracks[1]
        clip = owned.clip_slots[0].clip
        assert clip.end_marker == 8.0
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]

        repeated = _realize(client, "project_realization_create")
        assert repeated["success"] is False, repeated
        assert repeated["attempt_id"] == created["attempt_id"]
        assert len(workflow.live.song.tracks) == 2
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 1

        unchanged = _realize(client, "project_realization_update")
        assert unchanged["success"] is False, unchanged
        assert not workflow.native_apply_calls

        _ok(
            client, "score_set_dynamics", score_id=1, region={"start_bar": 1, "end_bar": 1}, level=6
        )
        revised = _realize(client, "project_realization_update")
        assert revised["success"] is False, revised
        assert not workflow.native_apply_calls
        assert workflow.managed_calls.count("sunny_managed_update_notes") == 0
        assert owned.clip_slots[0].clip is clip and clip.end_marker == 8.0
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
