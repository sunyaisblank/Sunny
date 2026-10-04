"""Independent compact reply admission through the actual public MCP/TCP path."""

from __future__ import annotations

import json

from test_mcp_native_device_workflow import _source
from test_mcp_native_device_workflow import device_workflow as device_workflow
from test_mcp_realization_workflow import _author, _literal_notes, _ok, _revision


def test_large_retained_descriptor_does_not_consume_compact_final_device_budget(request):
    """A valid 2.1MiB retained tag is in the plan, but absent from compact native evidence."""
    workflow = request.getfixturevalue("device_workflow")
    tag = "retained-descriptor:" + "x" * (2200 * 1024)
    with workflow.process() as client:
        _author(client, workflow)
        selected = _source(client)
        _ok(client, "set_semantic_descriptors", profile_id=1, tags=[tag])
        _ok(client, "workspace_save", path=workflow.server_workspace)
        revision = _revision(client)
        before_calls = list(workflow.managed_calls)
        plan = client.call(
            "project_realization_plan",
            score_id=1,
            expected_project_revision=revision,
            parts=[{"part_id": 1, "source_selections": selected}],
            routing=[],
        )
        assert plan.get("success") is True, plan.get("error")
        assert plan["mutation_dispatched"] is False
        assert workflow.managed_calls[len(before_calls) :] == ["sunny_managed_context"]
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == []
        assert len(workflow.live.song.tracks) == 1
        retained = plan["plan"]["parts"][0]["source_plan"]["retained_profile"]
        assert retained["semantic"]["tags"] == [tag]
        apply_arguments = {"plan": plan["plan"], "explicit_plan_approval": True}
        request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "tools/call",
            "params": {"name": "project_realization_apply", "arguments": apply_arguments},
        }
        assert len(json.dumps(request).encode()) < 4 * 1024 * 1024
        applied = client.call("project_realization_apply", **apply_arguments)
        assert applied.get("success") is True, applied.get("error")
        assert applied["selected_contract_completed"] is True
        assert applied["project_revision"] == revision
        # The declared retained tag may occupy the returned plan once; it must
        # not be copied into final finite native Device proof or phase summaries.
        final_text = json.dumps(applied)
        assert len(final_text.encode()) < 64 * 1024
        assert tag not in final_text
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
        assert workflow.managed_calls.count("sunny_managed_insert_device") == 1
        assert len(workflow.native_device_insertions) == 1
        track = workflow.live.song.tracks[1]
        assert [native.class_name for native in track._devices] == ["Drift"]
        assert [(p.original_name, p.value) for p in track._devices[0].parameters[1:5]] == [
            ("LP Freq", 0.5),
            ("Env 1 Attack", 0.5),
            ("Env 1 Decay", 0.5),
            ("Env 1 Release", 0.5),
        ]
        assert _literal_notes(track.clip_slots[0].clip) == [
            (1, 60, 0.0, 1.0, 80.0),
            (2, 67, 2.0, 0.5, 72.0),
        ]
