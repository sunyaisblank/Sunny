"""Saved physical intent recovers after simultaneous offline Score/Timbre/Mix edits.

The actual stdio product and TCP bridge run unchanged. Only external native
objects are modelled: provider document replacement copies their saved values
into new handles and IDs. It supplies no qualification of real Live persistence,
edition availability, historical native identity, or DSP equivalence.
"""

from __future__ import annotations

import copy
import json
import math
from decimal import Decimal

import pytest
from test_managed_devices import device
from test_mcp_historical_part_recovery import _adopt as _adopt_clip
from test_mcp_historical_part_recovery import _assert_terminal_ack
from test_mcp_historical_part_recovery import _preview as _preview_clip
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import (
    _assert_native_effect_values,
    _effect_selections,
    _effects,
    _native_writes,
    _parameters,
    _phase_requests,
)
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_recovery_workflow import _replace_provider_document
from test_mcp_note_population_workflow import _events
from test_mcp_realization_workflow import _author, _literal_notes, _ok, _realize, _revision

_HISTORIC = "retained_verified_realization"
_DEVICE_PREVIEW = "project_realization_preview_device_adoption"
_DEVICE_ADOPT = "project_realization_adopt_devices"
_FINAL_NOTES = [
    (101, 60, 0.0, 1.0, 90.0),
    (102, 67, 2.0, 0.5, 72.0),
    (103, 64, 4.0, 1.0, 76.0),
]


def _attempts(workflow):
    return json.loads(workflow.history_ledger().read_text())["attempts"]


def _documents(client):
    return _ok(client, "get_project_json", score_id=1)["documents"]


def _device_call(client, source, effects, tool=_DEVICE_PREVIEW, **overrides):
    arguments = {
        "score_id": 1,
        "part_id": 1,
        "expected_project_revision": _revision(client),
        "selections": source,
        "effect_selections": effects,
        "device_projection_source": _HISTORIC,
    }
    arguments.update(overrides)
    return client.call(tool, **arguments)


def _old_physical(track):
    """Literal independent native values: source1200 and saved effect targets."""
    source = _parameters(track._devices[0])
    assert source["LP Freq"].value == 0.5
    assert Decimal(source["LP Freq"].oracle(source["LP Freq"].value).split()[0]) == 1200
    _assert_native_effect_values(track)
    eq = _parameters(track._devices[3])
    for name, target in (("1 Frequency A", "733"), ("1 Gain A", "-5"), ("1 Resonance A", "1.75")):
        observed = Decimal(eq[name].oracle(eq[name].value).split()[0])
        assert abs(observed - Decimal(target)) <= Decimal("0.01")


def _final_physical(track):
    """Check literal native display curves within the caller's physical tolerances."""
    expected = (
        (("LP Freq", "2450", "0.05"),),
        (("Gain", "3", "0.01"), ("Stereo Width", "100", "0.01")),
        (("Gain", "0", "0.01"), ("Stereo Width", "150", "0.01")),
        (
            ("Scale", "100", "0.01"),
            ("Output Gain", "0", "0.01"),
            ("1 Frequency A", "1200", "0.01"),
            ("1 Gain A", "3", "0.01"),
            ("1 Resonance A", "2.75", "0.01"),
        ),
    )
    for native, controls in zip(track._devices, expected):
        parameters = _parameters(native)
        for name, target, tolerance in controls:
            parameter = parameters[name]
            observed = Decimal(parameter.oracle(parameter.value).split()[0])
            assert abs(observed - Decimal(target)) <= Decimal(tolerance)
    for native in track._devices[1:3]:
        parameters = _parameters(native)
        assert parameters["Balance"].value == 0.5
        assert parameters["Channel Mode"].value == 1.0
        assert parameters["Mono"].value == 1.0 and parameters["Mute"].value == 0.0
    eq = track._devices[3]
    assert eq.global_mode == 0 and eq.edit_mode is False and eq.oversample is False
    assert _parameters(eq)["1 Filter Type A"].value == 3.0


def _saved_then_offline_edits(client, workflow):
    """Make a complete real native chain, then edit all IR while native state stays old."""
    _author(client, workflow)
    source = _source(client)
    effects, width_id, eq_id = _effect_selections(client)
    created = _realize(client, "project_realization_create")
    assert created["success"], created
    source_authored = _timbre(client, source)
    assert source_authored["success"], source_authored
    effect_authored = _effects(client, effects)
    assert effect_authored["success"] and effect_authored["selected_effect_phases_completed"], (
        effect_authored
    )
    track, clip = workflow.live.song.tracks[1], workflow.live.song.tracks[1].clip_slots[0].clip
    _old_physical(track)
    saved = copy.deepcopy(_attempts(workflow))
    snapshot = max(saved, key=lambda entry: entry["dispatch_ordinal"])
    assert snapshot["intent"]["attempt_id"] == effect_authored["attempt_id"]
    assert (
        snapshot["bindings"][-1]["observation"]["device_identity"]["cohort"][0]["device_key"]
        == "source_1"
    )
    prior = _ok(client, "project_realization_inspect", attempt_id=effect_authored["attempt_id"])

    # Publish the final desired documents only. No native tool or temporary
    # prior-IR rollback occurs between these edits and external epoch change.
    _ok(client, "score_modify_note", score_id=1, event_id=_events(client)[0]["id"], velocity=90)
    _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
    _ok(
        client,
        "score_insert_note",
        score_id=1,
        part_id=1,
        bar=2,
        offset={"n": 0, "d": 1},
        pitch={"letter": "E", "accidental": 0, "octave": 4},
        duration={"n": 1, "d": 4},
        velocity=76,
    )
    _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[2], denominator=2)
    _ok(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2450.0)
    _ok(client, "set_channel_input_trim", graph_id=1, channel_id=1, input_trim_db=3.0)
    _ok(
        client,
        "replace_mix_effect",
        graph_id=1,
        effect_id=width_id,
        configuration={"effect_type": "stereo", "width": 1.5},
    )
    _ok(
        client,
        "replace_mix_effect",
        graph_id=1,
        effect_id=eq_id,
        configuration={
            "effect_type": "eq",
            "bands": [{"frequency": 1200.0, "gain": 3.0, "q": 2.75, "type": 0}],
        },
    )
    final_revision = _revision(client)
    final_documents = copy.deepcopy(_documents(client))
    _ok(client, "workspace_save", path=workflow.server_workspace)
    assert _attempts(workflow) == saved
    assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
    assert clip.end_marker == 4.0
    _old_physical(track)
    return {
        "source": source,
        "effects": effects,
        "width_id": width_id,
        "eq_id": eq_id,
        "saved_attempts": saved,
        "snapshot": snapshot,
        "terminal": copy.deepcopy(prior["receipt"]),
        "revision": final_revision,
        "documents": final_documents,
        "old_track": track,
        "old_clip": clip,
    }


def _recover_clip_and_final_score(client, workflow, state, track, clip):
    """Explicitly approve old music, then apply the final Score without touching devices."""
    assert _revision(client) == state["revision"] and _documents(client) == state["documents"]
    cohort, writes = tuple(track.devices), copy.deepcopy(_native_writes(track))
    parameters = tuple(tuple(native.parameters) for native in track._devices)
    mixer_parameters = (track.mixer_device.volume, track.mixer_device.panning)
    mixer_values = tuple(parameter.value for parameter in mixer_parameters)
    old_notes = dict(clip._notes)
    clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    clip._user_follow_actions = {"next": True}
    before = copy.deepcopy(_attempts(workflow))
    default = _preview_clip(client)
    assert default["success"] and not default["eligible_for_explicit_adoption"], default
    selected = _preview_clip(client, historical=True)
    assert selected["success"] and selected["eligible_for_explicit_adoption"], selected
    assert selected["historical_projection_attempt"] == state["snapshot"]["intent"]["attempt_id"]
    assert not selected["authority_granted"] and _attempts(workflow) == before
    assert _native_writes(track) == writes
    adopted = _adopt_clip(client, selected)
    assert adopted["success"] and adopted["history_saved"], adopted
    assert not adopted["receipt"]["journal"]["native_mutation_started"]
    assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 80.0), (102, 67, 2.0, 0.5, 72.0)]
    assert _native_writes(track) == writes
    revised = _realize(client, "project_realization_update")
    assert revised["success"] and revised["selected_clip_phases_completed"], revised
    assert len(revised["phase_attempt_ids"]) == 3
    assert _literal_notes(clip) == _FINAL_NOTES
    assert clip.end_marker == 8.0 and (clip.signature_numerator, clip.signature_denominator) == (
        2,
        2,
    )
    assert all(clip._notes[note_id] is note for note_id, note in old_notes.items())
    assert tuple(track.devices) == cohort
    assert tuple(tuple(native.parameters) for native in track._devices) == parameters
    assert (track.mixer_device.volume, track.mixer_device.panning) == mixer_parameters
    assert tuple(parameter.value for parameter in mixer_parameters) == mixer_values
    assert _native_writes(track) == writes
    assert _documents(client) == state["documents"] and _revision(client) == state["revision"]
    return revised


def _assert_retained_intents(preview, state):
    """Targets and tolerances come from immutable saved requests, not current IR."""
    assert preview["device_projection_source"] == _HISTORIC
    assert preview["current_owning_physical_intent_verified"] is False
    assert preview["current_authored_values_require_separate_writes"] is True
    assert preview["device_history_attempt"] == state["snapshot"]["intent"]["attempt_id"]
    assert (
        preview["saved_device_identity_fingerprint"]
        == state["snapshot"]["bindings"][-1]["observation"]["device_identity_fingerprint"]
    )
    retained = preview["retained_device_intents"]
    assert [selection["chain_index"] for selection in retained] == [0, 1, 2, 3]
    assert [selection["device_key"] for selection in retained] == [
        "source_1",
        "mix_1_channel_1_trim",
        f"mix_1_effect_{state['width_id']:x}",
        f"mix_1_effect_{state['eq_id']:x}",
    ]
    physical = [
        {
            intent["capability_id"]: (intent["target"], intent["tolerance"])
            for intent in member["physical_intents"]
        }
        for member in retained
    ]
    assert physical[0] == {
        "drift.lp.frequency": (1200.0, 0.05),
        "drift.env.1.attack": (250.0, 0.05),
        "drift.env.1.decay": (250.0, 0.05),
        "drift.env.1.release": (1000.0, 0.05),
    }
    assert physical[1] == {
        "utility.gain": (-7.5, 0.01),
        "utility.width": (100.0, 0.01),
        "utility.balance": (0.0, 0.0),
    }
    assert physical[2] == {
        "utility.gain": (0.0, 0.01),
        "utility.width": (125.0, 0.01),
        "utility.balance": (0.0, 0.0),
    }
    assert physical[3] == {
        "eq8.scale": (100.0, 0.01),
        "eq8.output_gain": (0.0, 0.01),
        "eq8.band.1.frequency": (733.0, 0.01),
        "eq8.band.1.gain": (-5.0, 0.01),
        "eq8.band.1.q": (1.75, 0.01),
    }
    assert {item["capability_id"]: item["label"] for item in retained[2]["enum_intents"]}[
        "utility.channel_mode"
    ] == "Stereo"
    assert {item["capability_id"]: item["label"] for item in retained[3]["enum_intents"]}[
        "eq8.band.1.type"
    ] == "Bell"
    assert retained[3]["property_intents"] == [{"property": "global_mode", "label": "Stereo"}]
    assert all(
        residual["opaque_state_observed"] is False
        for residual in preview["historical_device_residuals"]
    )
    eq_residual = preview["historical_device_residuals"][3]["saved_observed_modes"]
    assert eq_residual == {"global_mode": 0, "edit_mode": False, "oversample": False}


def test_offline_score_source_and_effect_edits_recover_saved_intent_then_apply_final_ir(
    request,
):
    """Fresh101/102 recover saved1200/733 first; final2450/1200 requires ordinary writes."""
    workflow = request.getfixturevalue("effect_workflow")
    with workflow.process() as client:
        state = _saved_then_offline_edits(client, workflow)
    track, clip = _replace_provider_document(workflow)
    assert track is not state["old_track"] and clip is not state["old_clip"]
    _old_physical(track)
    natives, note_count = tuple(track._devices), len(workflow.live.song.tracks)
    with workflow.process() as client:
        _recover_clip_and_final_score(client, workflow, state, track, clip)
        writes = copy.deepcopy(_native_writes(track))
        attempts = copy.deepcopy(_attempts(workflow))
        default = _device_call(
            client,
            state["source"],
            state["effects"],
            device_projection_source="current_owning_timbre",
        )
        assert default.get("success") is not True and default.get("error"), default
        assert _attempts(workflow) == attempts and _native_writes(track) == writes
        selected = _device_call(client, state["source"], state["effects"])
        assert selected["success"] and not selected["authority_granted"], selected
        _assert_retained_intents(selected, state)
        assert _attempts(workflow) == attempts and _native_writes(track) == writes
        approved = selected["preview"]
        common = {
            "preview": approved,
            "device_history_attempt": selected["device_history_attempt"],
            "explicit_adoption": True,
        }
        foreign = copy.deepcopy(approved)
        foreign["document_token"] = "foreign_document"
        # Ledger order follows random attempt IDs, not dispatch chronology.
        # Choose a genuinely older snapshot for the stale-history refusal.
        oldest_attempt = min(state["saved_attempts"], key=lambda entry: entry["dispatch_ordinal"])[
            "intent"
        ]["attempt_id"]
        assert oldest_attempt != selected["device_history_attempt"]
        invalid = [
            {"preview": foreign},
            {"expected_project_revision": state["revision"] - 1},
            {"part_id": 900},
            {"device_projection_source": "caller_defined"},
            {"desired_devices": [{"physical_intents": [{"target": 2450.0}]}]},
            {"device_history_attempt": "f" * 32},
            {"device_history_attempt": oldest_attempt},
        ]
        for change in invalid:
            rejected = _device_call(
                client,
                state["source"],
                state["effects"],
                _DEVICE_ADOPT,
                **(common | change),
            )
            assert rejected.get("success") is not True and rejected.get("error"), rejected
            assert _attempts(workflow) == attempts
            assert _native_writes(track) == writes and tuple(track._devices) == natives
        missing_source = dict(common)
        missing_source.pop("device_history_attempt")
        rejected = _device_call(
            client, state["source"], state["effects"], _DEVICE_ADOPT, **missing_source
        )
        assert rejected.get("success") is not True and rejected.get("error"), rejected
        assert _attempts(workflow) == attempts and _native_writes(track) == writes

        adopted = _device_call(client, state["source"], state["effects"], _DEVICE_ADOPT, **common)
        assert adopted["success"] and adopted["history_saved"], adopted
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert adopted["device_history_attempt"] == selected["device_history_attempt"]
        assert adopted["retained_device_intents"] == selected["retained_device_intents"]
        assert _native_writes(track) == writes and tuple(track._devices) == natives
        assert _documents(client) == state["documents"] and _revision(client) == state["revision"]
        assert _literal_notes(clip) == _FINAL_NOTES
        _old_physical(track)

        source_applied = _timbre(client, state["source"])
        assert (
            source_applied["success"] and source_applied["source_insertion_requested"] is False
        ), source_applied
        assert _parameters(natives[0])["LP Freq"].value == 0.75
        effects_applied = _effects(client, state["effects"])
        assert effects_applied["success"] and effects_applied["selected_effect_phases_completed"], (
            effects_applied
        )
        assert all(
            request["name"] != "sunny_managed_insert_device"
            for request in _phase_requests(workflow, effects_applied)
        )
        _final_physical(track)
        trim, eq = _parameters(natives[1]), _parameters(natives[3])
        assert trim["Gain"].value == 0.625
        assert eq["1 Frequency A"].value == 0.5
        assert eq["1 Gain A"].value == 0.625 and eq["1 Resonance A"].value == 0.75
        assert _native_writes(track) != writes
        assert tuple(track._devices) == natives and len(workflow.live.song.tracks) == note_count
        assert _literal_notes(clip) == _FINAL_NOTES and track.clip_slots[0].clip is clip
        assert clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
        assert clip._user_follow_actions == {"next": True}
        assert _documents(client) == state["documents"] and _revision(client) == state["revision"]
        old = _ok(
            client,
            "project_realization_inspect",
            attempt_id=state["snapshot"]["intent"]["attempt_id"],
        )
        _assert_terminal_ack(old["receipt"], state["terminal"])
        by_id = {entry["intent"]["attempt_id"]: entry for entry in _attempts(workflow)}
        for prior in state["saved_attempts"]:
            assert by_id[prior["intent"]["attempt_id"]] == prior
    assert len(workflow.native_device_insertions) == 4
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_devices") == 1


@pytest.mark.parametrize(
    "conflict", ["matching_final_not_saved", "extra_chain", "changed_logical_key"]
)
def test_historical_device_preview_refuses_native_or_current_logical_prefix_conflict_without_fence(
    request, conflict
):
    """New values, extra devices, or replaced effect keys cannot adopt saved intent."""
    workflow = request.getfixturevalue("effect_workflow")
    with workflow.process() as client:
        state = _saved_then_offline_edits(client, workflow)
    track, clip = _replace_provider_document(workflow)
    with workflow.process() as client:
        _recover_clip_and_final_score(client, workflow, state, track, clip)
        if conflict == "matching_final_not_saved":
            # Genuine external edits place the whole native chain at the new
            # final intent. These are deliberately not bridge setters or
            # authoring grants: saved1200/-7.5/125/733,-5,1.75 still differs.
            _parameters(track._devices[0])["LP Freq"]._value = 0.75
            _parameters(track._devices[1])["Gain"]._value = 0.625
            _parameters(track._devices[2])["Stereo Width"]._value = math.sqrt(0.75)
            eq = _parameters(track._devices[3])
            eq["1 Frequency A"]._value = 0.5
            eq["1 Gain A"]._value = 0.625
            eq["1 Resonance A"]._value = 0.75
            _final_physical(track)
            assert (
                Decimal(_parameters(track._devices[0])["LP Freq"].oracle(0.75).split()[0]) == 2450
            )
        elif conflict == "extra_chain":
            extra = device("Utility")
            extra._canonical_parent = track
            track._devices.append(extra)
            assert len(track._devices) == 5
        else:
            _ok(client, "remove_mix_effect", graph_id=1, effect_id=state["width_id"])
            replacement = _ok(
                client,
                "add_channel_effect",
                graph_id=1,
                channel_id=1,
                effect_type="stereo",
                width=1.5,
            )["effect_id"]
            assert replacement != state["width_id"]
            _ok(
                client,
                "reorder_mix_effects",
                graph_id=1,
                chain_path="channels[1].insert_chain",
                effect_ids=[replacement, state["eq_id"]],
            )
            state["effects"] = [
                {**selection, "authored_id": replacement}
                if selection["kind"] == "mix_effect"
                and selection["authored_id"] == state["width_id"]
                else selection
                for selection in state["effects"]
            ]
        before, writes = copy.deepcopy(_attempts(workflow)), copy.deepcopy(_native_writes(track))
        cohort, documents = tuple(track.devices), copy.deepcopy(_documents(client))
        if conflict == "matching_final_not_saved":
            current = _device_call(
                client,
                state["source"],
                state["effects"],
                device_projection_source="current_owning_timbre",
            )
            assert current["success"] and not current["authority_granted"], current
            assert current["current_owning_physical_intent_verified"] is True
            assert _attempts(workflow) == before and _native_writes(track) == writes
        rejected = _device_call(client, state["source"], state["effects"])
        assert rejected.get("success") is not True and rejected.get("error"), rejected
        if conflict == "changed_logical_key":
            assert "identities/classes/order" in rejected["error"], rejected
        assert _attempts(workflow) == before and _native_writes(track) == writes
        assert tuple(track.devices) == cohort and track.clip_slots[0].clip is clip
        assert _literal_notes(clip) == _FINAL_NOTES and _documents(client) == documents
    assert len(workflow.native_device_insertions) == 4
    assert "sunny_managed_adopt_devices" not in workflow.managed_calls
