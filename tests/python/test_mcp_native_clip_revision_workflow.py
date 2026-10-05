"""Combined authored revisions through real MCP/TCP and retained native objects.

Only the external provider is modelled. Restart keeps the same bridge and
native handles; these tests do not qualify real Live persistence or its ABI.
"""

from __future__ import annotations

import copy
import json

from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_device_workflow import device_workflow as device_workflow
from test_mcp_realization_workflow import _literal_notes, _ok, _realize, _revision

_GEOMETRY = "sunny_managed_update_clip_geometry"
_POPULATION = "sunny_managed_revise_note_population"
_NOTES = "sunny_managed_update_notes"


def _insert(client, *, bar, letter, offset, duration, velocity, articulation=None):
    arguments = {
        "score_id": 1,
        "part_id": 1,
        "bar": bar,
        "offset": offset,
        "pitch": {"letter": letter, "accidental": 0, "octave": 4},
        "duration": duration,
        "velocity": velocity,
    }
    if articulation is not None:
        arguments["articulation"] = articulation
    return _ok(client, "score_insert_note", **arguments)


def _events(client):
    score = _ok(client, "score_get_json", score_id=1)
    return [
        event
        for measure in score["parts"][0]["measures"]
        for event in measure["voices"][0]["events"]
        if event["type"] == "note_group"
    ]


def _author_old_part(client, workflow):
    """Real Score2bars compiles C@0/2⁄3,E@2⁄3/1⁄3,A@4/1,end8."""
    _ok(
        client,
        "score_create",
        title="One owning final native revision",
        total_bars=2,
        parts=[{"name": "Piano", "instrument_type": 47}],
    )
    _ok(client, "create_project", score_id=1)
    _insert(
        client,
        bar=1,
        letter="C",
        offset={"n": 0, "d": 1},
        duration={"n": 1, "d": 6},
        velocity=80,
    )
    _insert(
        client,
        bar=1,
        letter="E",
        offset={"n": 1, "d": 6},
        duration={"n": 1, "d": 12},
        velocity=72,
    )
    _insert(
        client,
        bar=2,
        letter="A",
        offset={"n": 0, "d": 1},
        duration={"n": 1, "d": 4},
        velocity=88,
    )
    selections = _source(client)
    saved = _ok(client, "workspace_save", path=workflow.server_workspace)
    assert saved["durability_confirmed"] and saved["native_history_available"]
    created = _realize(client, "project_realization_create")
    assert created["success"] and created["history_saved"], created
    authored = _timbre(client, selections)
    assert authored["success"] and authored["history_saved"], authored
    track = workflow.live.song.tracks[1]
    clip = track.clip_slots[0].clip
    assert _literal_notes(clip) == _old_literal()
    clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
    clip._user_follow_actions = {"next": True}
    return track, clip, _events(client), authored


def _old_literal():
    return [
        (1, 60, 0.0, 2.0 / 3.0, 80.0),
        (2, 64, 2.0 / 3.0, 1.0 / 3.0, 72.0),
        (3, 69, 4.0, 1.0, 88.0),
    ]


def _final_literal():
    return [
        (1, 62, 0.0, 2.0 / 3.0, 80.0),
        (2, 66, 2.0 / 3.0, 1.0 / 3.0, 72.0),
        (3, 69, 4.0, 1.0, 88.0),
        (4, 62, 8.0, 1.0, 76.0),
    ]


def _author_final_part(client, events):
    """Publish only final desired Score content before a single native update."""
    _ok(client, "score_insert_measures", score_id=1, after_bar=2, count=1)
    for event, letter, accidental in ((events[0], "D", 0), (events[1], "F", 1)):
        _ok(
            client,
            "score_modify_note",
            score_id=1,
            event_id=event["id"],
            pitch={"letter": letter, "accidental": accidental, "octave": 4},
        )
    _insert(
        client,
        bar=3,
        letter="D",
        offset={"n": 0, "d": 1},
        duration={"n": 1, "d": 4},
        velocity=76,
    )
    # 8/8 has the same measure length as4/4. Thus this independent meter change
    # cannot excuse changing any literal quarter-note start/duration above.
    _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[8], denominator=8)


def _phase_observer(workflow, track, clip, monkeypatch):
    """Observe actual returned native phases; retain the existing disk-fence oracle."""
    original_dispatch = workflow.surface._managed_registry.dispatch
    snapshots = []
    retained_mixer_parameters = (track.mixer_device.volume, track.mixer_device.panning)

    def dispatch(name, args):
        result = original_dispatch(name, args)
        if name in {_GEOMETRY, _POPULATION, _NOTES} and result.get("outcome") == "acknowledged":
            snapshots.append(
                {
                    "name": name,
                    "operation_id": args[0]["operation_id"],
                    "track": workflow.live.song.tracks[1],
                    "slot": track.clip_slots[0],
                    "clip": track.clip_slots[0].clip,
                    "extent": clip.end_marker,
                    "meter": (clip.signature_numerator, clip.signature_denominator),
                    "literal_notes": _literal_notes(clip),
                    "note_objects": dict(clip._notes),
                    # https://docs.cycling74.com/apiref/lom/track/ documents that
                    # Track.devices includes the mixer. Keep its complete
                    # actual cohort, but inspect native inserted-device parameter
                    # rows from this external provider's _devices only; public
                    # Mixer controls have their own concrete properties below.
                    "devices": tuple(track.devices),
                    "parameters": tuple(tuple(device.parameters) for device in track._devices),
                    "parameter_values": tuple(
                        tuple(parameter.value for parameter in device.parameters)
                        for device in track._devices
                    ),
                    "mixer_parameters": (track.mixer_device.volume, track.mixer_device.panning),
                    "retained_mixer_parameters": retained_mixer_parameters,
                    "mixer_values": (
                        track.mixer_device.volume.value,
                        track.mixer_device.panning.value,
                    ),
                    "mpe": copy.deepcopy(clip._user_mpe_expression),
                    "follow_actions": copy.deepcopy(clip._user_follow_actions),
                }
            )
        return result

    monkeypatch.setattr(workflow.surface._managed_registry, "dispatch", dispatch)
    return snapshots


def _assert_current_objects(snapshot, track, slot, clip, notes, devices, parameters, values):
    assert snapshot["track"] is track
    assert snapshot["slot"] is slot and snapshot["clip"] is clip
    assert all(snapshot["note_objects"][note_id] is note for note_id, note in notes.items())
    assert len(snapshot["devices"]) == len(devices)
    assert all(observed is retained for observed, retained in zip(snapshot["devices"], devices))
    assert snapshot["parameters"] == parameters
    assert snapshot["parameter_values"] == values
    assert snapshot["mixer_parameters"] == (track.mixer_device.volume, track.mixer_device.panning)
    assert all(
        observed is retained
        for observed, retained in zip(
            snapshot["mixer_parameters"], snapshot["retained_mixer_parameters"]
        )
    )
    assert snapshot["mixer_values"] == (0.85, 0.0)
    assert snapshot["mpe"] == {1: {"pressure": [0.1, 0.8]}}
    assert snapshot["follow_actions"] == {"next": True}


def _retained_phases(workflow, result, revision):
    attempts = json.loads(workflow.history_ledger().read_text())["attempts"]
    by_id = {entry["intent"]["attempt_id"]: entry for entry in attempts}
    assert len(set(result["phase_attempt_ids"])) == len(result["phase_attempt_ids"])
    phases = [by_id[token] for token in result["phase_attempt_ids"]]
    for phase in phases:
        assert phase["intent"]["project_revision"] == revision
        assert phase["intent"]["score_id"] == 1 and phase["intent"]["part_id"] == 1
        assert phase["dispatch_state"] == "may_have_sent"
    return phases


def test_combined_extent_meter_and_notes_preserve_objects_under_one_final_revision(
    request, monkeypatch
):
    """Final Score edit alone yields extend→notes→meter and reverse notes→shrink."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        track, clip, events, _ = _author_old_part(client, workflow)
        slot, notes, devices = track.clip_slots[0], dict(clip._notes), tuple(track.devices)
        parameters = tuple(tuple(device.parameters) for device in track._devices)
        values = tuple(
            tuple(parameter.value for parameter in device.parameters) for device in track._devices
        )
        snapshots = _phase_observer(workflow, track, clip, monkeypatch)
        _author_final_part(client, events)
        final_score = _ok(client, "score_get_json", score_id=1)
        final_revision = _revision(client)
        revised = _realize(client, "project_realization_update")
        assert revised["success"] and revised["selected_clip_phases_completed"], revised
        assert len(revised["phase_attempt_ids"]) == 3
        assert [snapshot["name"] for snapshot in snapshots] == [_GEOMETRY, _POPULATION, _GEOMETRY]
        assert [(snapshot["extent"], snapshot["meter"]) for snapshot in snapshots] == [
            (12.0, (4, 4)),
            (12.0, (4, 4)),
            (12.0, (8, 8)),
        ]
        assert [snapshot["literal_notes"] for snapshot in snapshots] == [
            _old_literal(),
            _final_literal(),
            _final_literal(),
        ]
        for snapshot in snapshots:
            _assert_current_objects(snapshot, track, slot, clip, notes, devices, parameters, values)
        assert workflow.native_apply_calls == [(1, 2)]
        phases = _retained_phases(workflow, revised, final_revision)
        assert [
            phase["intent"]["desired_projection"]["signature_numerator"] for phase in phases
        ] == [4, 4, 8]
        assert [len(phase["intent"]["desired_note_keys"]) for phase in phases] == [3, 4, 4]
        assert phases[0]["intent"]["desired_note_keys"] == [
            f"e{event['id']}_n0" for event in events
        ]
        assert (
            phases[1]["intent"]["desired_note_keys"][:3] == phases[0]["intent"]["desired_note_keys"]
        )
        assert _revision(client) == final_revision
        assert _ok(client, "score_get_json", score_id=1) == final_score
        _ok(client, "workspace_save", path=workflow.server_workspace)

    with workflow.process() as restarted:
        unchanged = _realize(restarted, "project_realization_update")
        assert unchanged["success"] and unchanged["phase_attempt_ids"] == [], unchanged
        assert unchanged["selected_clip_phases_completed"] and not unchanged["mutation_dispatched"]
        assert len(snapshots) == 3
        _ok(restarted, "score_delete_measures", score_id=1, bar=3, count=1)
        for event, letter in ((events[0], "C"), (events[1], "E")):
            _ok(
                restarted,
                "score_modify_note",
                score_id=1,
                event_id=event["id"],
                pitch={"letter": letter, "accidental": 0, "octave": 4},
            )
        _ok(restarted, "score_set_time_signature", score_id=1, bar=1, groups=[4], denominator=4)
        shrunk_score = _ok(restarted, "score_get_json", score_id=1)
        shrunk_revision = _revision(restarted)
        shrunk = _realize(restarted, "project_realization_update")
        assert shrunk["success"] and shrunk["selected_clip_phases_completed"], shrunk
        assert len(shrunk["phase_attempt_ids"]) == 2
        assert [snapshot["name"] for snapshot in snapshots[3:]] == [_POPULATION, _GEOMETRY]
        assert [(snapshot["extent"], snapshot["meter"]) for snapshot in snapshots[3:]] == [
            (12.0, (8, 8)),
            (8.0, (4, 4)),
        ]
        for snapshot in snapshots[3:]:
            assert snapshot["literal_notes"] == _old_literal()
            _assert_current_objects(snapshot, track, slot, clip, notes, devices, parameters, values)
        phases = _retained_phases(workflow, shrunk, shrunk_revision)
        assert phases[0]["intent"]["desired_projection"]["clip_end"] == 12.0
        assert phases[1]["intent"]["desired_projection"]["clip_end"] == 8.0
        assert phases[1]["intent"]["desired_note_keys"] == [
            f"e{event['id']}_n0" for event in events
        ]
        assert _revision(restarted) == shrunk_revision
        assert _ok(restarted, "score_get_json", score_id=1) == shrunk_score
        retained = _ok(restarted, "project_realization_inspect", attempt_id=revised["attempt_id"])
        assert retained["receipt"] == revised["receipt"]
    assert workflow.managed_calls.count(_GEOMETRY) == 3
    assert workflow.managed_calls.count(_POPULATION) == 2
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.native_apply_calls == [(1, 2), (1, 2)]


def test_lost_expand_reply_reconciles_original_then_restart_replan_skips_expand(
    request, monkeypatch
):
    """Expanded old attacks remain fenced; only remaining notes/meter phases run next."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        track, clip, events, _ = _author_old_part(client, workflow)
        slot, notes, devices = track.clip_slots[0], dict(clip._notes), tuple(track.devices)
        parameters = tuple(tuple(device.parameters) for device in track._devices)
        values = tuple(
            tuple(parameter.value for parameter in device.parameters) for device in track._devices
        )
        snapshots = _phase_observer(workflow, track, clip, monkeypatch)
        _author_final_part(client, events)
        final_revision = _revision(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = _GEOMETRY
        uncertain = _realize(client, "project_realization_update")
        assert not uncertain["success"] and not uncertain["selected_clip_phases_completed"], (
            uncertain
        )
        assert uncertain["phase_attempt_ids"] == [uncertain["attempt_id"]]
        assert uncertain["history_saved"] and uncertain["receipt"]["outcome"] == "indeterminate"
        assert len(snapshots) == 1
        assert snapshots[0]["extent"] == 12.0 and snapshots[0]["literal_notes"] == _old_literal()
        blocked = _realize(client, "project_realization_update")
        assert blocked["state"] == "reconciliation_required"
        assert (
            blocked["attempt_id"] == uncertain["attempt_id"] and not blocked["mutation_dispatched"]
        )
        assert len(snapshots) == 1 and not workflow.native_apply_calls

    with workflow.process() as restarted:
        blocked = _realize(restarted, "project_realization_update")
        assert (
            blocked["attempt_id"] == uncertain["attempt_id"] and not blocked["mutation_dispatched"]
        )
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert reconciled["history_saved"] and not reconciled["mutation_retried"]
        assert len(snapshots) == 1
        resumed = _realize(restarted, "project_realization_update")
        assert resumed["success"] and resumed["selected_clip_phases_completed"], resumed
        assert len(resumed["phase_attempt_ids"]) == 2
        assert uncertain["attempt_id"] not in resumed["phase_attempt_ids"]
        assert [snapshot["name"] for snapshot in snapshots] == [_GEOMETRY, _POPULATION, _GEOMETRY]
        assert snapshots[1]["literal_notes"] == snapshots[2]["literal_notes"] == _final_literal()
        assert snapshots[1]["meter"] == (4, 4) and snapshots[2]["meter"] == (8, 8)
        for snapshot in snapshots:
            _assert_current_objects(snapshot, track, slot, clip, notes, devices, parameters, values)
        assert _revision(restarted) == final_revision
        _retained_phases(workflow, resumed, final_revision)
        retained = _ok(restarted, "project_realization_inspect", attempt_id=uncertain["attempt_id"])
        assert retained["receipt"] == reconciled["receipt"]
    assert workflow.managed_calls.count(_GEOMETRY) == 2
    assert workflow.managed_calls.count(_POPULATION) == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.native_apply_calls == [(1, 2)]


def test_retained_chord_head_change_refuses_before_expansion_or_fence(request):
    """Adding a retained Event head cannot first publish an unrelated extent stage."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        track, clip, events, _ = _author_old_part(client, workflow)
        notes = dict(clip._notes)
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        _ok(client, "score_insert_measures", score_id=1, after_bar=2, count=1)
        _insert(
            client,
            bar=1,
            letter="G",
            offset={"n": 0, "d": 1},
            duration={"n": 1, "d": 6},
            velocity=76,
        )
        assert _events(client)[0]["id"] == events[0]["id"]
        final_revision = _revision(client)
        declined = _realize(client, "project_realization_update")
        assert (
            not declined["success"] and "UnsupportedChordCardinalityRevision" in declined["error"]
        ), declined
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        assert track.clip_slots[0].clip is clip and clip.end_marker == 8.0
        assert _literal_notes(clip) == _old_literal() and clip._notes == notes
        assert all(clip._notes[note_id] is note for note_id, note in notes.items())
        assert _revision(client) == final_revision
    assert workflow.managed_calls.count(_GEOMETRY) == 0
    assert workflow.managed_calls.count(_POPULATION) == 0
    assert not workflow.native_apply_calls


def test_compiled_articulation_tail_expands_to_full_sounding_endpoint(request, monkeypatch):
    """Notation ends12, but actual compiler extends to the G@11/2 endpoint13."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        track, clip, _, _ = _author_old_part(client, workflow)
        slot, notes, devices = track.clip_slots[0], dict(clip._notes), tuple(track.devices)
        parameters = tuple(tuple(device.parameters) for device in track._devices)
        values = tuple(
            tuple(parameter.value for parameter in device.parameters) for device in track._devices
        )
        assert any(device is track.mixer_device for device in devices)
        snapshots = _phase_observer(workflow, track, clip, monkeypatch)
        _ok(client, "score_insert_measures", score_id=1, after_bar=2, count=1)
        _ok(
            client,
            "score_set_articulation_mapping",
            score_id=1,
            part_id=1,
            articulation=2,
            mapping={"type": 3, "duration_scale": 2.0},
        )
        _insert(
            client,
            bar=3,
            letter="G",
            offset={"n": 3, "d": 4},
            duration={"n": 1, "d": 4},
            velocity=76,
            articulation=2,
        )
        final_revision = _revision(client)
        final_score = _ok(client, "score_get_json", score_id=1)
        # ableton_score.cpp chooses max(notation length, complete sounding tails).
        # The plan receives that existing compiler result without guessing12 or
        # shortening the actual articulated duration2 to fit notation end12.
        revised = _realize(client, "project_realization_update")
        assert revised["success"] and revised["selected_clip_phases_completed"], revised
        assert len(revised["phase_attempt_ids"]) == 2
        assert [snapshot["name"] for snapshot in snapshots] == [_GEOMETRY, _POPULATION]
        assert [(snapshot["extent"], snapshot["meter"]) for snapshot in snapshots] == [
            (13.0, (4, 4)),
            (13.0, (4, 4)),
        ]
        literal = _old_literal() + [(4, 67, 11.0, 2.0, 76.0)]
        assert snapshots[0]["literal_notes"] == _old_literal()
        assert snapshots[1]["literal_notes"] == literal
        for snapshot in snapshots:
            _assert_current_objects(snapshot, track, slot, clip, notes, devices, parameters, values)
        phases = _retained_phases(workflow, revised, final_revision)
        assert [phase["intent"]["desired_projection"]["clip_end"] for phase in phases] == [
            13.0,
            13.0,
        ]
        assert [len(phase["intent"]["desired_note_keys"]) for phase in phases] == [3, 4]
        assert track.clip_slots[0].clip is clip and clip.end_marker == 13.0
        assert _literal_notes(clip) == literal
        assert clip._notes[4] is snapshots[1]["note_objects"][4]
        assert _revision(client) == final_revision
        assert _ok(client, "score_get_json", score_id=1) == final_score
    assert workflow.managed_calls.count(_GEOMETRY) == 1
    assert workflow.managed_calls.count(_POPULATION) == 1
    assert not workflow.native_apply_calls
