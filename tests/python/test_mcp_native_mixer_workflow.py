"""Owning static Mixer controls through actual MCP/TCP and durable native fences.

Only external Live provider objects are modelled. The literal volume display
curve, mute/Activator coupling and solo consequences are supplied independently.
These tests cannot qualify actual Live DSP, host execution or Set persistence.
"""

from __future__ import annotations

import copy
import json
import threading

import pytest
from live_model import Song, Track
from test_managed_devices import Parameter
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import (
    _assert_native_effect_values,
    _effect_selections,
    _effects,
    _native_writes,
)
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_note_population_workflow import _events
from test_mcp_realization_workflow import _author, _literal_notes, _realize, _revision
from test_mcp_workspace_workflow import _call as _ok

DOMAINS = ["volume", "pan", "mute", "solo"]
UPDATE_METHOD = "sunny_managed_update_static_mixer"
ADOPT_METHOD = "sunny_managed_adopt_static_mixer"


@pytest.fixture
def mixer_workflow(request, monkeypatch):
    """Install genuine controls before creation grants retain their exact handles."""
    workflow = request.getfixturevalue("effect_workflow")
    original_create = Song.create_midi_track
    original_mute, original_solo = Track.mute, Track.solo
    workflow.native_mixer_gate_writes = []

    def create(song, index=-1):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        before = tuple(song.tracks)
        result = original_create(song, index)
        created = [track for track in song.tracks if all(track is not old for old in before)]
        assert len(created) == 1
        mixer = created[0].mixer_device
        # This source-provider law is an independent oracle, not a guessed
        # Live fader formula: -6dB is0.5; -35.75dB is0.25; initial0.375 is-25.66dB.
        volume = Parameter(
            "Track Volume", lambda value: f"{-40 + 272 * value**3:.2f} dB", value=0.375
        )
        volume._canonical_parent = mixer
        mixer._volume = volume
        return result

    def mute(track, value):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        workflow.native_mixer_gate_writes.append((track, "mute", value))
        original_mute.__set__(track, value)
        track.mixer_device.track_activator._value = 0.0 if value else 1.0

    def solo(track, value):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        workflow.native_mixer_gate_writes.append((track, "solo", value))
        original_solo.__set__(track, value)

    def muted_via_solo(track):
        cohort = (*track._song.tracks, *track._song.return_tracks)
        return any(native._solo for native in cohort) and not track._solo

    monkeypatch.setattr(Song, "create_midi_track", create)
    monkeypatch.setattr(Track, "mute", property(lambda track: original_mute.__get__(track), mute))
    monkeypatch.setattr(Track, "solo", property(lambda track: original_solo.__get__(track), solo))
    monkeypatch.setattr(Track, "muted_via_solo", property(muted_via_solo))
    return workflow


def _attempts(workflow):
    return json.loads(workflow.history_ledger().read_text())["attempts"]


def _mixer(client, tool, domains=DOMAINS, **extra):
    arguments = {
        "score_id": 1,
        "part_id": 1,
        "expected_project_revision": _revision(client),
        "domains": list(domains),
    }
    if "volume" in domains:
        arguments["volume_tolerance_db"] = 0.0
    return client.call(tool, **arguments, **extra)


def _mixer_preview(client, domains=DOMAINS, purpose="update"):
    return _mixer(client, "project_realization_preview_static_mixer", domains, purpose=purpose)


def _mixer_apply(client, preview, domains=DOMAINS, *, adoption=False, setwide=True):
    return _mixer(
        client,
        "project_realization_adopt_static_mixer"
        if adoption
        else "project_realization_apply_static_mixer",
        domains,
        preview=preview,
        explicit_current_mixer_approval=True,
        explicit_set_wide_audible_approval=setwide,
    )


def _author_values(client, *, level=-6.0, pan=-0.25, mute=True, solo=True):
    _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=level)
    _ok(client, "set_channel_pan", graph_id=1, channel_id=1, pan=pan)
    flags = _ok(client, "set_channel_flags", graph_id=1, channel_id=1, mute=mute, solo=solo)
    assert flags["mute"] is mute and flags["solo"] is solo


def _create_source(client, workflow):
    _author(client, workflow)
    source = _source(client)
    assert _realize(client, "project_realization_create")["success"]
    applied = _timbre(client, source)
    assert applied["success"], applied
    track = workflow.live.song.tracks[1]
    return track, track.clip_slots[0].clip


def _update_evidence(result):
    assert result["success"] and result["history_saved"], result
    assert result["dispatch_fenced"] and result["retry_authorized"] is False
    journal = result["receipt"]["journal"]
    assert journal["outcome"] == "acknowledged", journal
    return journal, journal["result"]["mixer_update"]


def test_four_owning_controls_noop_and_revision_preserve_notes_input_trim_effects(mixer_workflow):
    """Actual fader/flags change; already-authored trim/effects and native IDs persist."""
    workflow = mixer_workflow
    foreign = workflow.live.song.tracks[0]
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        selections, _, _ = _effect_selections(client)
        assert _effects(client, selections)["success"]
        _author_values(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        notes, devices = _literal_notes(clip), tuple(track._devices)
        effect_writes = copy.deepcopy(_native_writes(track))
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        before = _attempts(workflow)
        planned = _mixer(client, "project_realization_plan_static_mixer")
        assert planned["success"] and planned["mutation_dispatched"] is False, planned
        assert planned["plan"]["desired"] == {
            "volume": {"target": -6.0, "tolerance": 0.0},
            "pan": -0.25,
            "mute": True,
            "solo": True,
        }
        assert planned["plan"]["retained_mix"]["channels"][0]["input_trim"] == -7.5
        assert any(
            residual["document_pointer"] == "/channels/0/input_trim"
            for residual in planned["plan"]["residuals"]
        )
        preview = _mixer_preview(client)
        assert preview["success"] and preview["authority_granted"] is False, preview
        assert preview["mutation_dispatched"] is False
        candidate = preview["preview"]["preview"]["candidates"]["volume"]
        assert candidate["internal_value"] == 0.5 and candidate["display"] == "-6.00 dB"
        assert _attempts(workflow) == before and track.mixer_device.volume.writes == []
        unapproved_solo = _mixer_apply(client, preview["preview"], setwide=False)
        assert unapproved_solo["success"] is False
        assert _attempts(workflow) == before
        applied = _mixer_apply(client, preview["preview"])
        journal, evidence = _update_evidence(applied)
        assert journal["native_mutation_started"] is True
        assert evidence["returned_fields"] == DOMAINS
        assert evidence["readback"]["volume"]["display"] == "-6.00 dB"
        assert evidence["set_wide_audible_effect"] is True
        assert evidence["observed_untouched_state_preserved"] is True
        assert evidence["clip_and_note_ids_preserved"] is True
        assert track.mixer_device.volume.value == 0.5
        assert track.mixer_device.panning.value == -0.25
        assert track.mute is True and track.mixer_device.track_activator.value == 0.0
        assert track.solo is True and foreign.muted_via_solo is True
        assert foreign.mute is False and foreign.solo is False
        assert _literal_notes(clip) == notes and tuple(track._devices) == devices
        assert _native_writes(track) == effect_writes
        _assert_native_effect_values(track)
        noop = _mixer_apply(client, _mixer_preview(client)["preview"])
        noop_journal, noop_evidence = _update_evidence(noop)
        assert noop_journal["native_mutation_started"] is False
        assert noop_evidence["returned_fields"] == []
        assert track.mixer_device.volume.writes == [0.5]
        assert workflow.native_mixer_gate_writes == [(track, "mute", True), (track, "solo", True)]
        _author_values(client, level=-35.75, pan=0.5, mute=False, solo=False)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        revised = _mixer_apply(client, _mixer_preview(client)["preview"])
        _, revised_evidence = _update_evidence(revised)
        assert revised_evidence["returned_fields"] == DOMAINS
        assert revised_evidence["readback"]["volume"]["display"] == "-35.75 dB"
        assert track.mixer_device.volume.value == 0.25
        assert track.mixer_device.panning.value == 0.5
        assert track.mute is False and track.mixer_device.track_activator.value == 1.0
        assert track.solo is False and foreign.muted_via_solo is False
        assert _literal_notes(clip) == notes and tuple(track._devices) == devices
        assert _native_writes(track) == effect_writes
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
    assert workflow.native_apply_calls == []
    assert workflow.managed_calls.count(UPDATE_METHOD) == 3


def test_lost_mixer_reply_queries_original_attempt_after_restart_without_replay(mixer_workflow):
    """The retained ACK survives stdio restart; no second native volume setter occurs."""
    workflow = mixer_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-6.0)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        notes = _literal_notes(clip)
        preview = _mixer_preview(client, ["volume"])
        assert preview["success"], preview
        workflow.drop_next_reply = UPDATE_METHOD
        uncertain = _mixer_apply(client, preview["preview"], ["volume"])
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        attempt_id = uncertain["attempt_id"]
        assert track.mixer_device.volume.value == 0.5 and track.mixer_device.volume.writes == [0.5]
        blocked = _mixer_apply(client, preview["preview"], ["volume"])
        assert blocked["success"] is False and blocked["attempt_id"] == attempt_id, blocked
    with workflow.process() as restarted:
        reconciled = _ok(restarted, "project_realization_reconcile", attempt_id=attempt_id)
        assert reconciled["mutation_retried"] is False and reconciled["history_saved"]
        assert reconciled["receipt"]["outcome"] == "acknowledged", reconciled
        assert reconciled["attempt_id"] == attempt_id
        assert reconciled["receipt"]["journal"]["native_mutation_started"] is True
        assert workflow.live.song.tracks[1] is track and track.clip_slots[0].clip is clip
        assert _literal_notes(clip) == notes and track.mixer_device.volume.writes == [0.5]
    assert workflow.managed_calls.count(UPDATE_METHOD) == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_epoch_clip_adoption_needs_explicit_selected_mixer_adoption(mixer_workflow):
    """Reopened current Clip rights cannot silently grow into static Mixer authority."""
    workflow = mixer_workflow
    with workflow.process() as client:
        _, old_clip = _create_source(client, workflow)
        old_notes = _literal_notes(old_clip)
        _ok(client, "workspace_save", path=workflow.server_workspace)
    track, clip = _replace_provider_document(workflow)
    with workflow.process() as restarted:
        clip_preview = _preview(restarted)
        assert clip_preview["eligible_for_explicit_adoption"], clip_preview
        adopted_clip = _adopt(restarted, clip_preview["preview"])
        assert adopted_clip["success"], adopted_clip
        notes = _literal_notes(clip)
        assert [note[0] for note in notes] == [note[0] + 100 for note in old_notes]
        assert [note[1:] for note in notes] == [note[1:] for note in old_notes]
        _ok(restarted, "set_channel_level", graph_id=1, channel_id=1, level_db=-6.0)
        before = _attempts(workflow)
        unavailable = _mixer_preview(restarted, ["volume"])
        assert unavailable["success"] is False
        assert _attempts(workflow) == before and track.mixer_device.volume.writes == []
        preview = _mixer_preview(restarted, ["volume"], purpose="adopt")
        assert preview["success"] and preview["authority_granted"] is False, preview
        assert preview["preview"]["preview"]["current_authority_domains"] == []
        adopted = _mixer_apply(restarted, preview["preview"], ["volume"], adoption=True)
        assert adopted["success"] and adopted["history_saved"], adopted
        journal = adopted["receipt"]["journal"]
        assert journal["native_mutation_started"] is False
        assert journal["result"]["mixer_adoption"]["granted_domains"] == ["volume"]
        assert track.mixer_device.volume.value == 0.375 and track.mixer_device.volume.writes == []
        applied = _mixer_apply(
            restarted, _mixer_preview(restarted, ["volume"])["preview"], ["volume"]
        )
        _update_evidence(applied)
        assert track.mixer_device.volume.value == 0.5
        c_event = _events(restarted)[0]["id"]
        _ok(restarted, "score_modify_note", score_id=1, event_id=c_event, velocity=90)
        assert _realize(restarted, "project_realization_update")["success"]
        before_escalation = _attempts(workflow)
        unavailable_mute = _mixer_preview(restarted, ["mute"])
        assert unavailable_mute["success"] is False
        assert _attempts(workflow) == before_escalation
        assert track.mute is False and track.solo is False
        assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 90.0), (102, 67, 2.0, 0.5, 72.0)]
    assert workflow.managed_calls.count(ADOPT_METHOD) == 1
    assert workflow.managed_calls.count(UPDATE_METHOD) == 1


@pytest.mark.parametrize("drift", ["selected_volume", "panning_mode", "armed"])
def test_native_state_drift_declines_preview_before_fence(mixer_workflow, drift):
    """Read-only preview cannot bless a changed retained control or inadmissible Track."""
    workflow = mixer_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-6.0)
        notes = _literal_notes(clip)
        before = _attempts(workflow)
        if drift == "selected_volume":
            track.mixer_device.volume._value = 0.2
        elif drift == "panning_mode":
            track.mixer_device._panning_mode = 1
        else:
            track._arm = True
        declined = _mixer_preview(client, ["volume"])
        assert declined["success"] is False, declined
        assert _attempts(workflow) == before
        assert track.mixer_device.volume.writes == [] and _literal_notes(clip) == notes
    assert workflow.managed_calls.count(UPDATE_METHOD) == 0


def test_foreign_solo_drift_after_preview_retains_decline_without_setters(mixer_workflow):
    """Approved Set-wide cohort is immutable; foreign user edits stop all native setters."""
    workflow = mixer_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        _author_values(client)
        preview = _mixer_preview(client)
        assert preview["success"], preview
        notes = _literal_notes(clip)
        foreign = workflow.live.song.tracks[0]
        foreign._solo = True  # External native user edit, not an authorized product setter.
        declined = _mixer_apply(client, preview["preview"])
        assert declined["success"] is False and declined["history_saved"], declined
        assert declined["receipt"]["outcome"] == "declined"
        assert declined["receipt"]["journal"]["native_mutation_started"] is False
        assert track.mixer_device.volume.writes == []
        assert workflow.native_mixer_gate_writes == []
        assert foreign.solo is True and foreign.mute is False
        assert track.solo is False and track.mute is False
        assert _literal_notes(clip) == notes


def test_removed_part_retirement_uses_latest_verified_notes_and_mute_only(mixer_workflow):
    """Deleting owning Part1 retires its current native Track without deleting or formatting."""
    workflow = mixer_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        selections, _, _ = _effect_selections(client)
        assert _effects(client, selections)["success"]
        c_event = _events(client)[0]["id"]
        _ok(client, "score_modify_note", score_id=1, event_id=c_event, velocity=90)
        assert _realize(client, "project_realization_update")["success"]
        notes = _literal_notes(clip)
        assert notes == [(1, 60, 0.0, 1.0, 90.0), (2, 67, 2.0, 0.5, 72.0)]
        added = _ok(
            client, "score_add_part", score_id=1, name="Remaining Piano", instrument_type=47
        )
        assert added["part_id"] == 2
        _ok(
            client,
            "score_insert_note",
            score_id=1,
            part_id=2,
            bar=1,
            offset={"n": 0, "d": 1},
            pitch={"letter": "F", "accidental": 0, "octave": 4},
            duration={"n": 1, "d": 4},
            velocity=78,
        )
        second = client.call(
            "project_realization_create",
            score_id=1,
            part_id=2,
            expected_project_revision=_revision(client),
        )
        assert second["success"], second
        remaining = workflow.live.song.tracks[2]
        remaining_clip = remaining.clip_slots[0].clip
        assert _literal_notes(remaining_clip) == [(1, 65, 0.0, 1.0, 78.0)]
        devices, effect_writes = tuple(track._devices), copy.deepcopy(_native_writes(track))
        before_formatter = list(track.mixer_device.volume.calls)
        before_volume = list(track.mixer_device.volume.writes)
        clip._user_mpe_expression = {1: {"pressure": [0.2, 0.9]}}
        _ok(client, "score_remove_part", score_id=1, part_id=1)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        owning = _ok(client, "get_project_json", score_id=1)
        assert [part["id"] for part in owning["documents"]["score"]["parts"]] == [2]

        def forbidden_formatter(_):
            raise AssertionError("mute retirement must not call the volume formatter")

        track.mixer_device.volume.oracle = forbidden_formatter
        retirement_args = {
            "score_id": 1,
            "part_id": 1,
            "expected_project_revision": _revision(client),
            "explicit_mute_retirement": True,
        }
        retired = client.call("project_realization_retire_part", **retirement_args)
        journal, evidence = _update_evidence(retired)
        assert journal["native_mutation_started"] is True
        assert evidence["returned_fields"] == ["mute"]
        assert retired["retirement_action"] == "mute" and retired["native_objects_deleted"] is False
        assert retired["historical_projection_retained"] is True
        assert track.mute is True and track.mixer_device.track_activator.value == 0.0
        assert track.mixer_device.volume.calls == before_formatter
        assert track.mixer_device.volume.writes == before_volume
        assert workflow.live.song.tracks[1] is track and track.clip_slots[0].clip is clip
        assert _literal_notes(clip) == notes and tuple(track._devices) == devices
        assert _native_writes(track) == effect_writes
        assert clip._user_mpe_expression == {1: {"pressure": [0.2, 0.9]}}
        assert remaining.mute is False and remaining_clip is remaining.clip_slots[0].clip
        assert _literal_notes(remaining_clip) == [(1, 65, 0.0, 1.0, 78.0)]
        repeated = client.call("project_realization_retire_part", **retirement_args)
        noop_journal, noop_evidence = _update_evidence(repeated)
        assert noop_journal["native_mutation_started"] is False
        assert noop_evidence["returned_fields"] == []
        assert workflow.native_mixer_gate_writes == [(track, "mute", True)]
        assert track.mixer_device.volume.calls == before_formatter
        assert track.mixer_device.volume.writes == before_volume
