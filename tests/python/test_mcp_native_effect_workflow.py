"""Owning ordered effects through actual MCP/TCP, phase fences and native oracles.

Only external native provider objects are modelled. Provider document replacement
is an epoch boundary; these tests cannot qualify actual Live Set persistence,
edition availability or DSP equivalence.
"""

from __future__ import annotations

import copy
import json
import threading
from decimal import Decimal

import pytest
from live_model import DeviceParameter, Track
from test_managed_device_modes import EqDevice
from test_managed_devices import device, enum
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_native_song_settings_workflow import _song_apply, _song_preview
from test_mcp_realization_workflow import (
    NativeWorkflow,
    _author,
    _literal_notes,
    _ok,
    _realize,
    _revision,
)


@pytest.fixture
def effect_workflow(tmp_path, monkeypatch):
    """Expose independently chosen native curves and actual permuted mode labels."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    workflow.native_device_insertions = []
    workflow.native_managed_journals = []
    dispatch = workflow.surface._managed_registry.dispatch

    def observe_dispatch(name, args):
        result = dispatch(name, args)
        if isinstance(result, dict) and result.get("name") == name:
            workflow.native_managed_journals.append(copy.deepcopy(result))
        return result

    monkeypatch.setattr(workflow.surface._managed_registry, "dispatch", observe_dispatch)

    def insert(track, browser, index=None):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        assert index == len(track._devices)
        native = EqDevice() if browser == "EQ Eight" else device(browser)
        if browser == "Utility":
            # A literal nonlinear provider curve, independent of the resolver
            # and the authored width-to-percent conversion.
            native.parameters[3].oracle = lambda value: f"{200 * value * value:.2f} %"
            extra = [
                enum(name, ("On", "Off"), 0)
                for name in ("Left Inv", "Right Inv", "Bass Mono", "DC Filter")
            ]
            for parameter in extra:
                parameter._canonical_parent = native
            native._parameters += tuple(extra)
        native._canonical_parent = track
        track._devices.insert(index, native)
        workflow.native_device_insertions.append((track, native))

    monkeypatch.setattr(Track, "insert_device", insert)
    try:
        yield workflow
    finally:
        workflow.close()


def _effect_selections(client, enabled=True):
    """Author the real owning Mix trim, width and EQ through their MCP workflows."""
    _ok(client, "set_channel_input_trim", graph_id=1, channel_id=1, input_trim_db=-7.5)
    width = _ok(
        client, "add_channel_effect", graph_id=1, channel_id=1, effect_type="stereo", width=1.25
    )["effect_id"]
    eq = _ok(
        client,
        "add_channel_effect",
        graph_id=1,
        channel_id=1,
        effect_type="eq",
        enabled=enabled,
        bands=[{"frequency": 733.0, "gain": -5.0, "q": 1.75, "type": 0}],
    )["effect_id"]
    tolerance = {"decibels": 0.01, "hertz": 0.01, "quality_factor": 0.01, "percent": 0.01}
    # Declare the reverse order deliberately. Actual authoring follows signal
    # order: selected trim, then the authored width, then the authored EQ.
    selections = [
        {"kind": "mix_effect", "authored_id": eq, "tolerance": tolerance.copy()},
        {"kind": "mix_effect", "authored_id": width, "tolerance": tolerance.copy()},
        {"kind": "mix_input_trim", "authored_id": 1, "tolerance": tolerance.copy()},
    ]
    mix = _ok(client, "get_mix_json", graph_id=1)["mix_ir"]
    assert mix["channels"][0]["input_trim"] == -7.5
    return selections, width, eq


def _effects(client, selections, tool="project_realization_author_effects"):
    return client.call(
        tool,
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        effect_selections=selections,
    )


def _parameters(native):
    return {parameter.original_name: parameter for parameter in native.parameters}


def _native_writes(track):
    """Count literal external setters, including the native EQ mode property."""
    return [
        {
            "parameters": [list(parameter.writes) for parameter in native.parameters],
            "global_mode": list(getattr(native, "mode_writes", [])),
        }
        for native in track._devices
    ]


def _phase_requests(workflow, result):
    """Inspect the actual durable requests for the completed product phases."""
    attempts = json.loads(workflow.history_ledger().read_text())["attempts"]
    by_id = {attempt["intent"]["attempt_id"]: attempt for attempt in attempts}
    retained = [by_id[token] for token in result["phase_attempt_ids"]]
    assert all(attempt["dispatch_state"] == "may_have_sent" for attempt in retained)
    assert all(attempt["evidence"][-1]["outcome"] == "acknowledged" for attempt in retained)
    return [attempt["intent"]["prepared"]["request"] for attempt in retained]


def _assert_native_effect_values(track, *, width_square=0.625, frequency_square=0.13325):
    """Literal nonlinear values and enum ordinals come from the independent provider."""
    assert [native.class_name for native in track._devices] == [
        "Drift",
        "StereoGain",
        "StereoGain",
        "Eq8",
    ]
    trim, width, eq = track._devices[1:]
    trim_values, width_values, eq_values = map(_parameters, (trim, width, eq))
    assert trim_values["Gain"].value == 0.1875  # 24*0.1875 -12 = -7.5 dB.
    assert abs(trim_values["Stereo Width"].value ** 2 - 0.5) < 0.0001
    assert trim_values["Balance"].value == 0.5
    assert width_values["Gain"].value == 0.5
    assert abs(width_values["Stereo Width"].value ** 2 - width_square) < 0.0001
    assert width_values["Stereo Width"].value != width_square
    assert width_values["Balance"].value == 0.5
    for values in (trim_values, width_values):
        assert values["Channel Mode"].value == 1.0  # Actual labels put Stereo at1.
        assert values["Mono"].value == 1.0  # Actual labels put Off at1.
        assert values["Mute"].value == 0.0
        assert all(
            values[name].value == 1.0
            for name in ("Left Inv", "Right Inv", "Bass Mono", "DC Filter")
        )
    assert eq.global_mode == 0 and eq.edit_mode is False and eq.oversample is False
    assert eq_values["Adaptive Q"].value == 1.0
    assert abs(eq_values["Scale"].value ** 2 - 0.5) < 0.0001
    assert eq_values["Output Gain"].value == 0.5
    assert eq_values["1 Filter On A"].value == 0.0
    assert eq_values["1 Filter Type A"].value == 3.0  # Bell is actual index3.
    assert abs(eq_values["1 Frequency A"].value ** 2 - frequency_square) < 0.0001
    assert all(eq_values[f"{band} Filter On A"].value == 1.0 for band in range(2, 9))


def test_owning_trim_width_eq_phases_then_revision_retain_notes_and_devices(effect_workflow):
    """Real authored -7.5dB/125%/733Hz plans resolve nonlinear native values in order."""
    workflow = effect_workflow
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        selections, width_id, eq_id = _effect_selections(client)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, source)["success"]
        track = workflow.live.song.tracks[1]
        clip = track.clip_slots[0].clip
        notes = dict(clip._notes)
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        planned = _effects(client, selections, "project_realization_plan_effects")
        assert planned["success"] and planned["mutation_dispatched"] is False, planned
        assert [entry["device_browser_name"] for entry in planned["plan"]["entries"]] == [
            "Utility",
            "Utility",
            "EQ Eight",
        ]
        assert [entry["desired_chain_index"] for entry in planned["plan"]["entries"]] == [1, 2, 3]
        assert len(track._devices) == 1
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        stale = client.call(
            "project_realization_author_effects",
            score_id=1,
            part_id=1,
            expected_project_revision=_revision(client) + 1,
            effect_selections=selections,
        )
        assert stale["success"] is False and "Project revision changed" in stale["error"]
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before

        authored = _effects(client, selections)
        if not authored["success"]:
            journal_path = workflow.directory / "native-effect-failure.json"
            journal_path.write_text(json.dumps(workflow.native_managed_journals[-1], indent=2))
            phase_evidence = {
                "phase": authored.get("effect_phase"),
                "state": authored.get("state"),
                "receipt_error": authored.get("receipt", {}).get("error"),
                "native_outcome": workflow.native_managed_journals[-1].get("outcome"),
                "native_error": workflow.native_managed_journals[-1].get("error"),
                "native_journal_path": str(journal_path),
            }
        else:
            phase_evidence = authored
        assert authored["success"] and authored["selected_effect_phases_completed"], phase_evidence
        assert (
            authored["host_qualified"] is False and authored["dsp_equivalence_qualified"] is False
        )
        requests = _phase_requests(workflow, authored)
        assert [request["name"] for request in requests] == [
            "sunny_managed_insert_device",
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_parameters",
        ] * 2 + [
            "sunny_managed_insert_device",
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_modes",
            "sunny_managed_update_device_parameters",
            "sunny_managed_update_device_parameters",
        ]
        assert all(
            request["args"][0]["physical_intents"] == []
            for request in requests
            if request["name"] == "sunny_managed_insert_device"
        )
        natives = tuple(track._devices)
        _assert_native_effect_values(track)
        eq_values = _parameters(natives[3])
        # The independent provider prints these controls to two decimal places.
        # Admission promises .01 displayed units, not an arbitrary internal-knob
        # error. Its literal curves must also lie within that error plus half
        # the .01 display quantum; the normalized gain/Q substitutes fail both.
        gain, resonance = eq_values["1 Gain A"], eq_values["1 Resonance A"]
        assert abs(Decimal(gain.str_for_value(gain.value).removesuffix(" dB")) + 5) <= Decimal(
            ".01"
        )
        assert abs(Decimal(resonance.str_for_value(resonance.value)) - Decimal("1.75")) <= Decimal(
            ".01"
        )
        assert abs(24 * Decimal(str(gain.value)) - 7) <= Decimal(".015")
        assert abs(4 * Decimal(str(resonance.value)) ** 2 - Decimal("1.25")) <= Decimal(".015")
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]

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
        revised = _effects(client, selections)
        assert revised["success"] and revised["selected_effect_phases_completed"], revised
        assert all(
            request["name"] != "sunny_managed_insert_device"
            for request in _phase_requests(workflow, revised)
        )
        assert tuple(track._devices) == natives
        _assert_native_effect_values(track, width_square=0.75, frequency_square=0.25)
        assert eq_values["1 Frequency A"].value == 0.5
        assert eq_values["1 Gain A"].value == 0.625
        assert eq_values["1 Resonance A"].value == 0.75
        assert all(clip._notes[note_id] is note for note_id, note in notes.items())
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
        assert not workflow.native_apply_calls
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        retained = _ok(restarted, "project_realization_inspect", attempt_id=revised["attempt_id"])
        assert retained["receipt"] == revised["receipt"]
        assert tuple(track._devices) == natives and track.clip_slots[0].clip is clip
    assert len(workflow.native_device_insertions) == 4


def test_lost_effect_enable_reply_stops_then_queries_original_without_duplicate(effect_workflow):
    """One returned enable call cannot advance later phases or repeat its native insertion."""
    workflow = effect_workflow
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        selections, _, _ = _effect_selections(client)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, source)["success"]
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_update_device_modes"
        uncertain = _effects(client, selections)
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["effect_phase"] == "enable"
        assert uncertain["selected_effect_phases_completed"] is False
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        assert len(uncertain["phase_attempt_ids"]) == 2
        track = workflow.live.song.tracks[1]
        trim = track._devices[1]
        assert len(track._devices) == 2 and len(workflow.native_device_insertions) == 2
        assert _parameters(trim)["Device On"].writes == [1.0]
        assert all(not parameter.writes for parameter in trim.parameters[1:])
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        writes = _native_writes(track)
        blocked = _effects(client, selections)
        assert blocked["state"] == "reconciliation_required" and not blocked["mutation_dispatched"]
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert _native_writes(track) == writes and len(track._devices) == 2
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
    with workflow.process() as restarted:
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert reconciled["mutation_retried"] is False
        assert _native_writes(track) == writes and len(track._devices) == 2
        completed = _effects(restarted, selections)
        assert completed["success"] and completed["selected_effect_phases_completed"], completed
        assert track._devices[1] is trim
        _assert_native_effect_values(track)
        assert _literal_notes(track.clip_slots[0].clip) == [
            (1, 60, 0.0, 1.0, 80.0),
            (2, 67, 2.0, 0.5, 72.0),
        ]
        retained = _ok(restarted, "project_realization_inspect", attempt_id=uncertain["attempt_id"])
        assert retained["receipt"] == reconciled["receipt"]
    assert len(workflow.native_device_insertions) == 4
    assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_external_document_adopts_whole_chain_without_setters_then_revises(effect_workflow):
    """Current101/102 and a bypassed EQ require explicit Clip and whole-chain device grants."""
    workflow = effect_workflow
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        selections, width_id, _ = _effect_selections(client, enabled=False)
        created = _realize(client, "project_realization_create")
        assert created["success"]
        assert _timbre(client, source)["success"]
        authored = _effects(client, selections)
        assert authored["success"] and authored["effect_phase"] == "authored_bypass", authored
        assert workflow.live.song.tracks[1]._devices[3].is_active is False
        _ok(client, "workspace_save", path=workflow.server_workspace)
    current_track, current_clip = _replace_provider_document(workflow)
    natives = tuple(current_track._devices)
    current_clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    current_clip._user_follow_actions = {"next": True}
    writes = _native_writes(current_track)
    bypass_formatter_calls = [list(parameter.calls) for parameter in natives[3].parameters]
    with workflow.process() as restarted:
        refused = _effects(restarted, selections)
        assert refused["success"] is False
        assert (
            _native_writes(current_track) == writes and len(workflow.native_device_insertions) == 4
        )
        clip_adopted = _adopt(restarted, _preview(restarted)["preview"])
        assert clip_adopted["success"], clip_adopted
        assert clip_adopted["receipt"]["journal"]["native_mutation_started"] is False
        refused_without_device_grant = _effects(restarted, selections)
        assert refused_without_device_grant["success"] is False
        assert _native_writes(current_track) == writes

        device_preview = _timbre(
            restarted,
            source,
            "project_realization_preview_device_adoption",
            effect_selections=selections,
        )
        assert device_preview["success"] and device_preview["authority_granted"] is False, (
            device_preview
        )
        approved = device_preview["preview"]
        assert len(approved["preview"]["devices"]) == 4
        assert approved["preview"]["devices"][3]["authored_bypass"] is True
        assert approved["preview"]["devices"][3]["physical_intents"] == []
        assert [
            list(parameter.calls) for parameter in natives[3].parameters
        ] == bypass_formatter_calls
        assert _native_writes(current_track) == writes
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        foreign = copy.deepcopy(approved)
        foreign["document_token"] = "foreign_document"
        rejected = _timbre(
            restarted,
            source,
            "project_realization_adopt_devices",
            effect_selections=selections,
            preview=foreign,
            explicit_adoption=True,
        )
        assert rejected["success"] is False
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        assert _native_writes(current_track) == writes

        device_adopted = _timbre(
            restarted,
            source,
            "project_realization_adopt_devices",
            effect_selections=selections,
            preview=approved,
            explicit_adoption=True,
        )
        assert device_adopted["success"] and device_adopted["history_saved"], device_adopted
        assert device_adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert _native_writes(current_track) == writes
        assert [
            list(parameter.calls) for parameter in natives[3].parameters
        ] == bypass_formatter_calls
        assert _literal_notes(current_clip) == [
            (101, 60, 0.0, 1.0, 80.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        _ok(
            restarted,
            "replace_mix_effect",
            graph_id=1,
            effect_id=width_id,
            configuration={"effect_type": "stereo", "width": 1.5},
        )
        revised = _effects(restarted, selections)
        assert revised["success"] and revised["selected_effect_phases_completed"], revised
        assert tuple(current_track._devices) == natives
        assert all(
            request["name"] != "sunny_managed_insert_device"
            for request in _phase_requests(workflow, revised)
        )
        _assert_native_effect_values(current_track, width_square=0.75)
        assert natives[3].is_active is False
        assert _parameters(natives[3])["Device On"].writes[-2:] == [1.0, 0.0]
        assert _literal_notes(current_clip) == [
            (101, 60, 0.0, 1.0, 80.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        old = _ok(restarted, "project_realization_inspect", attempt_id=created["attempt_id"])
        assert old["receipt"] == created["receipt"]
        old_bypass = _ok(
            restarted, "project_realization_inspect", attempt_id=authored["attempt_id"]
        )
        assert old_bypass["receipt"] == authored["receipt"]
    assert current_track.clip_slots[0].clip is current_clip
    assert current_clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
    assert current_clip._user_follow_actions == {"next": True}
    assert not workflow.native_apply_calls and len(workflow.native_device_insertions) == 4


def test_set_wide_barrier_never_attributes_older_song_token_to_effect_phase(effect_workflow):
    """A lost no-op Song ACK blocks effects without a new fence or phase attempt ID."""
    workflow = effect_workflow
    song = workflow.live.song
    song.loop_start = 0.0
    song.loop_length = 8.0
    tempo = DeviceParameter("Song Tempo", minimum=20.0, maximum=999.0, value=120.0)
    tempo._canonical_parent = song.master_track.mixer_device
    song.master_track.mixer_device.song_tempo = tempo
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        selections, _, _ = _effect_selections(client)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, source)["success"]
        workflow.drop_next_reply = "sunny_managed_apply_song_settings"
        uncertain = _song_apply(client, _song_preview(client))
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        track = song.tracks[1]
        writes = _native_writes(track)
        attempts = json.loads(workflow.history_ledger().read_text())["attempts"]
        managed_calls = list(workflow.managed_calls)
        blocked = _effects(client, selections)
        assert blocked["state"] == "set_wide_reconciliation_required", blocked
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert blocked["mutation_dispatched"] is False
        assert blocked["selected_effect_phases_completed"] is False
        assert blocked["phase_attempt_ids"] == []
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == attempts
        assert _native_writes(track) == writes
        assert workflow.managed_calls == managed_calls + ["sunny_managed_context"]
        assert len(track._devices) == 1 and len(workflow.native_device_insertions) == 1
        assert _literal_notes(track.clip_slots[0].clip) == [
            (1, 60, 0.0, 1.0, 80.0),
            (2, 67, 2.0, 0.5, 72.0),
        ]
        actual = _ok(client, "project_realization_reconcile", attempt_id=uncertain["attempt_id"])
        assert actual["receipt"]["outcome"] == "acknowledged"
        assert actual["receipt"]["journal"]["native_mutation_started"] is False
        assert actual["mutation_retried"] is False
        assert _native_writes(track) == writes and len(workflow.native_device_insertions) == 1
