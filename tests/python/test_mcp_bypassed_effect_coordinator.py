"""Whole-project bypass completion joins typed configuration to the actual final cohort.

The MCP process, TCP bridge and durable fences are real. External native objects
provide independent values; no Live DSP, edition or Set persistence is qualified.
"""

from __future__ import annotations

import copy
import json
from decimal import Decimal

from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import (
    _effect_selections,
    _effects,
    _native_writes,
    _parameters,
)
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_project_realization_coordinator import _apply, _plan, _succeeded
from test_mcp_realization_workflow import _author, _literal_notes, _ok, _realize, _revision


def _attempts(workflow):
    return json.loads(workflow.history_ledger().read_text())["attempts"]


def test_whole_selected_bypassed_effect_configures_new_targets_before_off_and_final_proof(request):
    """New1200/+3/2.75 has successful physical receipts; fresh Off cannot echo that proof."""
    workflow = request.getfixturevalue("effect_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        effects, _, eq_id = _effect_selections(client, enabled=False)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, source)["success"]
        old = _effects(client, effects)
        assert old["success"] and old["selected_effect_phases_completed"], old
        track = workflow.live.song.tracks[1]
        slot, clip = track.clip_slots[0], track.clip_slots[0].clip
        notes = dict(clip._notes)
        devices = tuple(track._devices)
        parameters = tuple(tuple(native.parameters) for native in devices)
        source_writes = copy.deepcopy(_native_writes(track)[0])
        mixer = (track.mixer_device.volume, track.mixer_device.panning)
        mixer_values = tuple(parameter.value for parameter in mixer)
        native = devices[3]
        assert native.is_active is False
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        _ok(
            client,
            "replace_mix_effect",
            graph_id=1,
            effect_id=eq_id,
            configuration={
                "effect_type": "eq",
                "enabled": False,
                "bands": [{"frequency": 1200.0, "gain": 3.0, "q": 2.75, "type": 0}],
            },
        )
        revision = _revision(client)
        documents = copy.deepcopy(_ok(client, "get_project_json", score_id=1)["documents"])
        selection = {
            "score_id": 1,
            "expected_project_revision": revision,
            "parts": [{"part_id": 1, "source_selections": source, "effect_selections": effects}],
            "routing": [],
        }
        before, writes = copy.deepcopy(_attempts(workflow)), copy.deepcopy(_native_writes(track))
        planned = _plan(client, selection)
        assert planned["success"] and planned["mutation_dispatched"] is False, planned
        assert _attempts(workflow) == before and _native_writes(track) == writes
        completed = _succeeded(workflow, _apply(client, planned["plan"]))
        assert completed["project_revision"] == revision
        assert len(set(completed["phase_attempt_ids"])) == len(completed["phase_attempt_ids"])
        by_id = {record["intent"]["attempt_id"]: record for record in _attempts(workflow)}
        eq_phases = []
        for attempt_id in completed["phase_attempt_ids"]:
            record = by_id[attempt_id]
            assert record["intent"]["project_revision"] == revision
            assert record["dispatch_state"] == "may_have_sent"
            assert record["evidence"][-1]["outcome"] == "acknowledged"
            request = record["intent"]["prepared"]["request"]
            payload = request["args"][0]
            if payload.get("device_key") == f"mix_1_effect_{eq_id:x}":
                eq_phases.append((record, request, payload))
        assert [request["name"] for _, request, _ in eq_phases] == [
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_parameters",
            "sunny_managed_update_device_parameters",
            "sunny_managed_update_device_modes",
        ]
        assert eq_phases[0][2]["enum_intents"] == [{"capability_id": "eq8.enabled", "label": "On"}]
        assert eq_phases[-1][2]["enum_intents"] == [
            {"capability_id": "eq8.enabled", "label": "Off"}
        ]
        setup = {
            intent["capability_id"]: (intent["target"], intent["tolerance"])
            for intent in eq_phases[2][2]["physical_intents"]
        }
        authored = {
            intent["capability_id"]: (intent["target"], intent["tolerance"])
            for intent in eq_phases[3][2]["physical_intents"]
        }
        assert setup == {"eq8.scale": (100.0, 0.01), "eq8.output_gain": (0.0, 0.01)}
        assert authored == {
            "eq8.band.1.frequency": (1200.0, 0.01),
            "eq8.band.1.gain": (3.0, 0.01),
            "eq8.band.1.q": (2.75, 0.01),
        }
        for record, _, _ in eq_phases[2:4]:
            update = record["evidence"][-1]["journal"]["result"]["device_update"]
            assert all(readback["matches_intent"] is True for readback in update["readbacks"])
            assert update["clip_and_note_ids_preserved"] is True
        eq_controls = _parameters(native)
        for name, expected in (
            ("1 Frequency A", "1200"),
            ("1 Gain A", "3"),
            ("1 Resonance A", "2.75"),
        ):
            actual = Decimal(eq_controls[name].oracle(eq_controls[name].value).split()[0])
            assert abs(actual - Decimal(expected)) <= Decimal("0.01")
        assert native.is_active is False
        assert eq_controls["Device On"].writes[-2:] == [1.0, 0.0]
        assert _native_writes(track)[0] == source_writes
        assert tuple(track._devices) == devices
        assert tuple(tuple(device.parameters) for device in devices) == parameters
        assert (track.mixer_device.volume, track.mixer_device.panning) == mixer
        assert tuple(parameter.value for parameter in mixer) == mixer_values
        assert track.clip_slots[0] is slot and slot.clip is clip
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        assert all(clip._notes[note_id] is note for note_id, note in notes.items())
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
        assert _revision(client) == revision
        assert _ok(client, "get_project_json", score_id=1)["documents"] == documents
        proof = completed["final_native_observations"][0]
        last_binding = eq_phases[-1][0]["bindings"][-1]
        last_disabled = last_binding["observation"]
        guard = proof["binding_guard"]
        assert guard["bridge_instance"] == last_binding["context"]["bridge_instance"]
        assert guard["document_token"] == last_binding["context"]["document_token"]
        assert guard["project_key"] == last_binding["project_key"]
        assert guard["binding_key"] == "part_1"
        assert guard["track_index"] == 1 and guard["slot_index"] == 0
        assert guard["note_count"] == 2 and guard["device_count"] == 4
        for field in (
            "content_fingerprint",
            "note_identity_fingerprint",
            "device_identity_fingerprint",
        ):
            assert guard[field] == last_disabled[field]
        assert proof["selected_device_physical_intent_verified"] is False
        assert proof["active_selected_device_physical_intent_verified"] is True
        assert proof["bypassed_physical_targets_verified_before_disable"] is True
        assert proof["current_bypassed_display_verified"] is False
        configured = proof["bypassed_device_configuration"]
        assert configured["device_history_attempt"] == eq_phases[-1][0]["intent"]["attempt_id"]
        assert configured["device_identity_fingerprint"] == guard["device_identity_fingerprint"]
        assert len(configured["devices"]) == 1
        selected = configured["devices"][0]
        assert selected["device_key"] == f"mix_1_effect_{eq_id:x}"
        assert selected["chain_index"] == 3
        assert {
            intent["capability_id"]: (intent["target"], intent["tolerance"])
            for intent in selected["physical_intents"]
        } == setup | authored
    assert len(workflow.native_device_insertions) == 4
    assert not workflow.native_apply_calls
