"""Owning physical Timbre plans through real stdio/TCP, native fixtures and durable history."""

from __future__ import annotations

import threading

import pytest
from live_model import Track
from test_managed_devices import device
from test_mcp_realization_workflow import (
    NativeWorkflow,
    _author,
    _literal_notes,
    _ok,
    _realize,
    _revision,
)


@pytest.fixture
def device_workflow(tmp_path, monkeypatch):
    """Use independent nonlinear native parameter oracles at the insertion boundary."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    workflow.native_device_insertions = []

    def insert(track, browser, index=None):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        assert browser == "Drift" and index == len(track._devices)
        native = device(browser)
        native._canonical_parent = track
        track._devices.insert(index, native)
        workflow.native_device_insertions.append((track, native))

    monkeypatch.setattr(Track, "insert_device", insert)
    try:
        yield workflow
    finally:
        workflow.close()


def _source(client):
    _ok(
        client,
        "set_sound_source",
        profile_id=1,
        source_type="subtractive",
        filter_cutoff=1200.0,
        attack=250.0,
        decay=250.0,
        sustain=0.4,
        release=1000.0,
    )
    pairs = (
        ("source.filter.cutoff", "LP Freq", "drift.lp.frequency", 20.0, 20000.0),
        ("source.amplifier.stages[0].duration", "Env 1 Attack", "drift.env.1.attack", 0.0, 10000.0),
        ("source.amplifier.stages[1].duration", "Env 1 Decay", "drift.env.1.decay", 0.0, 10000.0),
        (
            "source.amplifier.stages[2].duration",
            "Env 1 Release",
            "drift.env.1.release",
            0.0,
            10000.0,
        ),
    )
    for path, name, _, minimum, maximum in pairs:
        _ok(
            client,
            "map_timbre_parameter",
            profile_id=1,
            ir_path=path,
            device_index=0,
            parameter_name=name,
            device_name="Drift",
            source_min=minimum,
            source_max=maximum,
            target_min=0.0,
            target_max=1.0,
            value_property="display_value",
        )
    return [
        {"source_path": path, "capability_id": capability, "tolerance": 0.05}
        for path, _, capability, _, _ in pairs
    ]


def _timbre(client, selections, tool="project_realization_author_timbre", **extra):
    return client.call(
        tool,
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        selections=selections,
        **extra,
    )


def test_owning_drift_hz_ms_then_note_revision_and_restart_preserve_native_ids(device_workflow):
    """Native0.5 gives1200Hz/250ms/250ms/1000ms; later2450Hz requires0.75, not normalization."""
    workflow = device_workflow
    with workflow.process() as client:
        _author(client, workflow)
        selections = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        track = workflow.live.song.tracks[1]
        slot, clip = track.clip_slots[0], track.clip_slots[0].clip
        initial = _literal_notes(clip)
        authored = _timbre(client, selections)
        assert authored["success"] and authored["history_saved"], authored
        assert authored["source_insertion_requested"] is True
        assert authored["native_knob_only"] and authored["dsp_equivalence_qualified"] is False
        native = track._devices[0]
        assert [(parameter.name, parameter.value) for parameter in native.parameters[1:5]] == [
            ("LP Freq", 0.5),
            ("Env 1 Attack", 0.5),
            ("Env 1 Decay", 0.5),
            ("Env 1 Release", 0.5),
        ]
        update = authored["receipt"]["journal"]["result"]["device_update"]
        assert [item["display_value"] for item in update["readbacks"]] == [1200, 250, 250, 1000]
        assert _literal_notes(clip) == initial
        _ok(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2450.0)
        revised = _timbre(client, selections)
        assert revised["success"] and revised["source_insertion_requested"] is False, revised
        assert native.parameters[1].value == 0.75
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        notes = _realize(client, "project_realization_update")
        assert notes["success"], notes
        assert _literal_notes(clip) == [(1, 62, 0.0, 1.0, 80.0), (2, 69, 2.0, 0.5, 72.0)]
        assert native.parameters[1].value == 0.75
        assert native.voice_mode_index == 1 and native.voice_count_index == 2
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        again = _timbre(restarted, selections)
        assert again["success"] and again["source_insertion_requested"] is False, again
        assert track._devices[0] is native and native.parameters[1].value == 0.75
        retained = _ok(restarted, "project_realization_inspect", attempt_id=authored["attempt_id"])
        assert retained["receipt"] == authored["receipt"]
    assert track.clip_slots[0] is slot and slot.clip is clip
    assert len(workflow.native_device_insertions) == 1
    assert workflow.managed_calls.count("sunny_managed_insert_device") == 1
    assert workflow.managed_calls.count("sunny_managed_update_device_parameters") == 2


def test_device_lost_reply_is_query_only_and_cannot_insert_again(device_workflow):
    """After native insertion/setters return, dropping its frame never creates another Drift."""
    workflow = device_workflow
    with workflow.process() as client:
        _author(client, workflow)
        selections = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_insert_device"
        uncertain = _timbre(client, selections)
        assert uncertain["success"] is False and uncertain["receipt"]["outcome"] == "indeterminate"
        native = workflow.live.song.tracks[1]._devices[0]
        blocked = _timbre(client, selections)
        assert (
            blocked["state"] == "reconciliation_required"
            and blocked["mutation_dispatched"] is False
        )
        assert blocked["attempt_id"] == uncertain["attempt_id"]
    with workflow.process() as restarted:
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert reconciled["mutation_retried"] is False
        revised = _timbre(restarted, selections)
        assert revised["success"] and revised["source_insertion_requested"] is False, revised
    assert workflow.live.song.tracks[1]._devices[0] is native
    assert len(workflow.native_device_insertions) == 1
    assert workflow.managed_calls.count("sunny_managed_insert_device") == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_native_device_user_edit_is_preserved_and_inspection_cannot_refresh_guard(device_workflow):
    """An external native cutoff change survives ordinary inspect and authoring declines."""
    workflow = device_workflow
    with workflow.process() as client:
        _author(client, workflow)
        selections = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, selections)["success"]
        native = workflow.live.song.tracks[1]._devices[0]
        native.parameters[1]._value = 0.75
        writes = list(native.parameters[1].writes)
        inspected = _ok(client, "project_realization_inspect", score_id=1, part_id=1)
        assert inspected["current_native_state_observed"]
        declined = _timbre(client, selections)
        assert declined["success"] is False and declined["receipt"]["outcome"] == "declined", (
            declined
        )
        assert declined["receipt"]["journal"]["native_mutation_started"] is False
        assert native.parameters[1].value == 0.75 and native.parameters[1].writes == writes
    assert len(workflow.native_device_insertions) == 1
