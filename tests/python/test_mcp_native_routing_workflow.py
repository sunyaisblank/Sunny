"""Literal owning routing/Return/send/Group joins over the actual MCP and TCP.

Only external native provider objects are modelled. Native routing objects use
the pinned advertised collection/attached_object boundary; no name is authority.
These tests cannot qualify an actual Live host, DSP or saved Set persistence.
"""

from __future__ import annotations

import copy
import json
import math
import threading

import pytest
from live_model import RoutingTypeCategory, Song, Track
from test_managed_devices import Parameter
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import (
    _create_source,
    _mixer_apply,
    _mixer_preview,
)
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_recovery_workflow import _adopt, _preview
from test_mcp_realization_workflow import _literal_notes
from test_mcp_workspace_workflow import _call as _ok

APPLY_METHOD = "sunny_managed_apply_routing"


@pytest.fixture
def routing_workflow(request, monkeypatch):
    """Actual native callback counts, nonlinear sends and current Group topology."""
    workflow = request.getfixturevalue("mixer_workflow")
    song = workflow.live.song
    song.loop_start, song.loop_length = 0.0, 8.0
    workflow.native_return_calls = []
    workflow.native_route_calls = []
    workflow.routing_type_clamp = False
    original_create = Song.create_return_track
    original_midi_create = Song.create_midi_track
    original_types = Track._output_types
    type_property, channel_property = Track.output_routing_type, Track.output_routing_channel

    class NativeSend(Parameter):
        def _write(self, value):
            assert threading.get_ident() == workflow.main_thread._thread.ident
            super()._write(value)

    def parameter(mixer, ordinal):
        # Independent physical oracle: -48+48*sqrt(value). A requested -24dB
        # requires value0.25; direct linear normalisation would give0.5.
        native = NativeSend(
            f"Send {ordinal}", lambda value: f"{-48 + 48 * math.sqrt(value):.2f} dB", value=0.375
        )
        native._canonical_parent = mixer
        return native

    def complete_sends(current, protected=None):
        count = len(current.return_tracks)
        for track in (*current.tracks, *current.return_tracks):
            mixer = track.mixer_device
            previous = tuple(mixer.sends)
            mixer._sends = previous + tuple(
                parameter(mixer, ordinal) for ordinal in range(len(previous), count)
            )
            # New native provider sends have the independent formatter, while
            # every already-existing handle/value is retained exactly.
            mixer._sends = tuple(
                native
                if isinstance(native, Parameter)
                or (protected and ordinal < len(protected.get(track, ())))
                else parameter(mixer, ordinal)
                for ordinal, native in enumerate(mixer.sends)
            )

    # One unrelated Return exists before any managed Part authority. Creation
    # must append precisely one, preserving its existing sends and user state.
    original_create(song)
    complete_sends(song)

    def create(current):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        workflow.native_return_calls.append("create_return_track")
        protected = {
            track: tuple(track.mixer_device.sends)
            for track in (*current.tracks, *current.return_tracks)
        }
        result = original_create(current)
        complete_sends(current, protected)
        return result

    def create_midi(current, index=-1):
        protected = {
            track: tuple(track.mixer_device.sends)
            for track in (*current.tracks, *current.return_tracks)
        }
        result = original_midi_create(current, index)
        complete_sends(current, protected)
        return result

    def output_types(track):
        result = original_types(track)
        if track.has_audio_output:
            result += [
                (
                    ("current_group", id(group)),
                    group.name,
                    RoutingTypeCategory.parent_group_track,
                    group,
                )
                for group in track._song.tracks
                if group._kind == "group" and group is not track
            ]
        return result

    def write_type(track, value):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        workflow.native_route_calls.append((track, "output_type", value.attached_object))
        if not workflow.routing_type_clamp:
            type_property.__set__(track, value)

    def write_channel(track, value):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        workflow.native_route_calls.append((track, "output_channel", value.display_name))
        channel_property.__set__(track, value)

    monkeypatch.setattr(Song, "create_return_track", create)
    monkeypatch.setattr(Song, "create_midi_track", create_midi)
    monkeypatch.setattr(Track, "_output_types", output_types)
    monkeypatch.setattr(
        Track,
        "output_routing_type",
        property(lambda track: type_property.__get__(track), write_type),
    )
    monkeypatch.setattr(
        Track,
        "output_routing_channel",
        property(lambda track: channel_property.__get__(track), write_channel),
    )
    monkeypatch.setattr(
        Track, "is_foldable", property(lambda track: track._kind == "group"), raising=False
    )
    monkeypatch.setattr(
        Track,
        "is_grouped",
        property(lambda track: getattr(track, "_group_actual", None) is not None),
    )
    monkeypatch.setattr(
        Track,
        "group_track",
        property(lambda track: getattr(track, "_group_actual", None)),
        raising=False,
    )
    return workflow


def _attempts(workflow):
    return json.loads(workflow.history_ledger().read_text())["attempts"]


def _revision(client, score=1):
    return _ok(client, "get_project_json", score_id=score)["project"]["revision"]


def _route(client, tool, kind, *, score=1, part=1, **arguments):
    return client.call(
        f"project_realization_{tool}_routing",
        score_id=score,
        part_id=part,
        expected_project_revision=_revision(client, score),
        kind=kind,
        **arguments,
    )


def _routing_preview(client, kind, **arguments):
    result = _route(client, "preview", kind, **arguments)
    assert result["success"] and result["mutation_dispatched"] is False, result
    assert result["preview"]["native_mutation_started"] is False
    return result["preview"]


def _routing_apply(client, kind, preview, *, explicit=True, **arguments):
    return _route(
        client,
        "apply",
        kind,
        preview=preview,
        explicit_current_routing_approval=explicit,
        **arguments,
    )


def _evidence(result, *, group=False):
    assert result["success"] and result["history_saved"] and result["dispatch_fenced"], result
    assert result["retry_authorized"] is False
    journal = result["receipt"]["journal"]
    assert journal["outcome"] == "acknowledged", journal
    return journal, journal["result"]["group_adoption" if group else "routing"]


def _aux(client):
    return _ok(client, "create_aux_bus", graph_id=1, name="Authored room")["aux_bus_id"]


def _send(client, aux, *, channel=1, pre=False, level=-24.0):
    _ok(
        client,
        "set_channel_send",
        graph_id=1,
        channel_id=channel,
        aux_id=aux,
        level_db=level,
        pre_fader=pre,
    )


def _second_part(client, workflow):
    part = _ok(client, "score_add_part", score_id=1, name="Second Piano", instrument_type=47)
    assert part["part_id"] == 2
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
    result = client.call(
        "project_realization_create",
        score_id=1,
        part_id=2,
        expected_project_revision=_revision(client),
    )
    assert result["success"], result
    return workflow.live.song.tracks[2]


def _second_project(client, *, create=False):
    score = _ok(
        client,
        "score_create",
        title="Foreign owned project",
        total_bars=1,
        parts=[{"name": "Piano", "instrument_type": 47}],
    )
    assert score["score_id"] == 2
    _ok(client, "create_project", score_id=2)
    _ok(
        client,
        "score_insert_note",
        score_id=2,
        part_id=1,
        bar=1,
        offset={"n": 0, "d": 1},
        pitch={"letter": "D", "accidental": 0, "octave": 4},
        duration={"n": 1, "d": 4},
        velocity=74,
    )
    if create:
        result = client.call(
            "project_realization_create",
            score_id=2,
            part_id=1,
            expected_project_revision=_revision(client, 2),
        )
        assert result["success"], result


def _native_group(workflow, members, name="Current native strings"):
    song = workflow.live.song
    group = Track(song, name, "group")
    for ordinal in range(len(song.return_tracks)):
        group._mixer._add_send(f"Existing send {ordinal}")
    song._tracks.append(group)
    for member in members:
        member._group_actual = group
    return group


def _group_arguments(workflow, group_id, group, selected):
    return {
        "group_id": group_id,
        "group_track_index": workflow.live.song.tracks.index(group),
        "selector": {"track_index": workflow.live.song.tracks.index(selected), "slot_index": 0},
    }


def _owning_group(client, members=(1,)):
    return _ok(
        client,
        "create_group_bus",
        graph_id=1,
        name="Authored strings",
        member_channel_ids=list(members),
    )["group_bus_id"]


def _refresh_clip(client):
    preview = _preview(client)
    assert preview["success"], preview
    adopted = _adopt(client, preview["preview"])
    assert adopted["success"], adopted
    return adopted


def _external_baseline(client, track):
    # External user state, never a managed callback or guessed route token.
    track._output_type, track._output_channel = ("external",), None
    _refresh_clip(client)


def _routing_candidates(client):
    result = client.call(
        "project_realization_inspect_routing",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
    )
    assert result["success"] and result["mutation_dispatched"] is False, result
    actual = result["candidates"]
    assert actual["native_mutation_started"] is False and actual["authority_origin"] == "none"
    return actual["frame"]["mixers"][actual["observation"]["track_index"]]["routing"]


def _destination_id(candidates, kind="main", index=None):
    return next(
        route["identifier"]
        for route in candidates["available_types"]
        if route["attached_target"] == {"kind": kind, "index": index}
    )


def test_return_append_preserves_three_part_ids_and_requires_fresh_mixer_grant(routing_workflow):
    """Two owning Parts plus another project receive validated send-population overlays."""
    workflow = routing_workflow
    with workflow.process() as client:
        first, first_clip = _create_source(client, workflow)
        second = _second_part(client, workflow)
        _second_project(client, create=True)
        foreign = workflow.live.song.tracks[3]
        parts = (first, second, foreign)
        clips = tuple(track.clip_slots[0].clip for track in parts)
        notes = tuple(_literal_notes(clip) for clip in clips)
        old_sends = tuple(track.mixer_device.sends[0] for track in parts)
        old_return = workflow.live.song.return_tracks[0]
        first_clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        first_clip._user_follow_actions = {"next": True}
        aux = _aux(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        before = _attempts(workflow)
        plan = _route(client, "plan", "create_return", aux_id=aux)
        assert plan["success"] and plan["mutation_dispatched"] is False, plan
        preview = _routing_preview(client, "create_return", aux_id=aux)
        assert _attempts(workflow) == before and workflow.native_return_calls == []
        declined = _routing_apply(client, "create_return", preview, explicit=False, aux_id=aux)
        assert declined["success"] is False and _attempts(workflow) == before
        result = _routing_apply(client, "create_return", preview, aux_id=aux)
        journal, evidence = _evidence(result)
        assert journal["native_mutation_started"] is True
        assert evidence["progress"]["returned"] == ["create_return_track", "return_name"]
        assert len(evidence["affected_observations"]) == 3
        assert len(workflow.live.song.return_tracks) == 2
        assert workflow.live.song.return_tracks[0] is old_return
        assert workflow.native_return_calls == ["create_return_track"]
        for track, clip, original_notes, send in zip(parts, clips, notes, old_sends):
            assert track.clip_slots[0].clip is clip and _literal_notes(clip) == original_notes
            assert track.mixer_device.sends[0] is send and send.value == 0.375
            assert len(track.mixer_device.sends) == 2
        assert first_clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert first_clip._user_follow_actions == {"next": True}
        unavailable = _mixer_preview(client, ["volume"])
        assert unavailable["success"] is False
        fresh = _mixer_preview(client, ["volume"], purpose="adopt")
        assert fresh["success"], fresh
        _evidence_mixer = _mixer_apply(client, fresh["preview"], ["volume"], adoption=True)
        assert _evidence_mixer["success"], _evidence_mixer
        # A successful foreign Part update after the Return delta proves Root
        # uses that Part's authorized overlay rather than its obsolete sends.
        _ok(
            client,
            "score_transpose",
            score_id=2,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        updated = client.call(
            "project_realization_update",
            score_id=2,
            part_id=1,
            expected_project_revision=_revision(client, 2),
        )
        assert updated["success"], updated
        assert _literal_notes(clips[2]) == [(1, 64, 0.0, 1.0, 74.0)]
    assert workflow.managed_calls.count(APPLY_METHOD) == 1


def test_current_return_adoption_and_nonlinear_send_touch_only_selected_native_knob(
    routing_workflow,
):
    """Explicit current Return adoption uses no setter; -24dB requires literal0.25."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        aux = _aux(client)
        _send(client, aux)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        returned = workflow.live.song.return_tracks[0]
        notes, devices = _literal_notes(clip), tuple(track._devices)
        preview = _routing_preview(client, "adopt_return", aux_id=aux, return_index=0)
        adopted = _routing_apply(client, "adopt_return", preview, aux_id=aux, return_index=0)
        journal, _ = _evidence(adopted)
        assert journal["native_mutation_started"] is False and workflow.native_return_calls == []
        selected = track.mixer_device.sends[0]
        preview = _routing_preview(client, "send_level", aux_id=aux, send_tolerance_db=0.0)
        candidate = preview["preview"]["selected"]["candidate"]
        assert candidate["internal_value"] == 0.25 and candidate["display"] == "-24.00 dB"
        applied = _routing_apply(client, "send_level", preview, aux_id=aux, send_tolerance_db=0.0)
        journal, evidence = _evidence(applied)
        assert journal["native_mutation_started"] is True
        assert evidence["send_readback"]["display"] == "-24.00 dB"
        assert (
            evidence["tap_policy_observed"] is False and evidence["logical_send_complete"] is False
        )
        assert selected.value == 0.25 and selected.writes == [0.25]
        assert workflow.live.song.return_tracks[0] is returned
        assert tuple(track._devices) == devices and _literal_notes(clip) == notes
        noop = _routing_apply(
            client,
            "send_level",
            _routing_preview(client, "send_level", aux_id=aux, send_tolerance_db=0.0),
            aux_id=aux,
            send_tolerance_db=0.0,
        )
        noop_journal, _ = _evidence(noop)
        assert noop_journal["native_mutation_started"] is False and selected.writes == [0.25]


def test_conflicting_aux_tap_intent_refuses_every_routing_kind_before_fence(routing_workflow):
    """One Return-global Pre/Post setting cannot faithfully satisfy two conflicting sends."""
    workflow = routing_workflow
    with workflow.process() as client:
        _create_source(client, workflow)
        _second_part(client, workflow)
        aux = _aux(client)
        _send(client, aux, channel=1, pre=False)
        _send(client, aux, channel=2, pre=True)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        before = _attempts(workflow)
        calls = list(workflow.managed_calls)
        for tool in ("plan", "preview"):
            result = _route(client, tool, "create_return", aux_id=aux)
            assert result["success"] is False and "Pre/Post" in result["error"], result
        assert _attempts(workflow) == before and workflow.managed_calls == calls
        assert workflow.native_return_calls == [] and len(workflow.live.song.return_tracks) == 1


def test_main_output_type_and_channel_use_actual_destination_and_fresh_phase(routing_workflow):
    """Actual Main handle, separate fresh channel phase and exact no-op are observed."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        _external_baseline(client, track)
        notes, devices = _literal_notes(clip), tuple(track._devices)
        route_id = _destination_id(_routing_candidates(client))
        preview = _routing_preview(client, "output_type", route_identifier=route_id)
        applied = _routing_apply(client, "output_type", preview, route_identifier=route_id)
        journal, evidence = _evidence(applied)
        assert journal["native_mutation_started"] is True and evidence["desired_match"] is True
        assert track.output_routing_type.attached_object is workflow.live.song.master_track
        channel_id = _routing_candidates(client)["available_channels"][0]["identifier"]
        preview = _routing_preview(client, "output_channel", route_identifier=channel_id)
        applied = _routing_apply(client, "output_channel", preview, route_identifier=channel_id)
        journal, _ = _evidence(applied)
        assert journal["native_mutation_started"] is False
        assert workflow.native_route_calls == [
            (track, "output_type", workflow.live.song.master_track)
        ]
        assert _literal_notes(clip) == notes and tuple(track._devices) == devices


def test_identical_main_name_with_foreign_attached_target_confers_no_authority(routing_workflow):
    """A native Return named Master remains a Return, regardless of advertised display name."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, _ = _create_source(client, workflow)
        foreign = workflow.live.song.return_tracks[0]
        foreign._name = "Master"
        # This is a current unrelated user edit. Explicit current Clip refresh
        # preserves it; it still cannot turn the selected Return into Main.
        _refresh_clip(client)
        route = next(
            item for item in track.available_output_routing_types if item.attached_object is foreign
        )
        before = _attempts(workflow)
        result = _route(client, "preview", "output_type", route_identifier=str(hash(route)))
        assert result["success"] is False and "TargetAssociationUnavailable" in result["error"], (
            result
        )
        assert _attempts(workflow) == before and workflow.native_route_calls == []
        assert route.display_name == "Master" and route.attached_object is foreign


def test_type_clamp_is_truthful_ack_and_stops_later_channel_phase(routing_workflow):
    """A returned type setter retaining Ext.Out cannot authorize a Main channel write."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        _external_baseline(client, track)
        notes = _literal_notes(clip)
        candidates = _routing_candidates(client)
        main_id = _destination_id(candidates)
        before_channel_id = candidates["available_channels"][0]["identifier"]
        preview = _routing_preview(client, "output_type", route_identifier=main_id)
        workflow.routing_type_clamp = True
        applied = _routing_apply(client, "output_type", preview, route_identifier=main_id)
        assert applied["success"] is False and applied["history_saved"], applied
        journal = applied["receipt"]["journal"]
        assert journal["outcome"] == "acknowledged" and journal["native_mutation_started"] is True
        assert journal["result"]["routing"]["desired_match"] is False
        before = _attempts(workflow)
        # The requested Main channel is not the actual currently advertised
        # Ext.Out channel. Never guess that successful setter return selected it.
        denied = _route(client, "preview", "output_channel", route_identifier=before_channel_id)
        assert denied["success"] is False, denied
        assert _attempts(workflow) == before and _literal_notes(clip) == notes
        assert (
            len(workflow.native_route_calls) == 1
            and workflow.native_route_calls[0][1] == "output_type"
        )


def test_group_bootstrap_grants_only_hierarchy_then_separate_clip_adoption(routing_workflow):
    """A real current Group is approved without Clip rights, setters or synthetic Track creation."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        group_id = _owning_group(client)
        group = _native_group(workflow, (track,))
        arguments = _group_arguments(workflow, group_id, group, track)
        notes, devices = _literal_notes(clip), tuple(track._devices)
        preview = _routing_preview(client, "adopt_group", **arguments)
        applied = _routing_apply(client, "adopt_group", preview, **arguments)
        journal, evidence = _evidence(applied, group=True)
        assert set(journal["result"]) == {"group_adoption"}
        assert journal["native_mutation_started"] is False
        assert (
            evidence["part_authority_granted"] is False
            and evidence["device_authority_granted"] is False
        )
        assert workflow.native_route_calls == [] and workflow.native_return_calls == []
        ordinary = client.call(
            "project_realization_update",
            score_id=1,
            part_id=1,
            expected_project_revision=_revision(client),
        )
        assert ordinary["success"] is False and workflow.native_apply_calls == [], ordinary
        _refresh_clip(client)
        group_route_id = _destination_id(
            _routing_candidates(client), "track", workflow.live.song.tracks.index(group)
        )
        preview = _routing_preview(client, "output_type", route_identifier=group_route_id)
        applied = _routing_apply(client, "output_type", preview, route_identifier=group_route_id)
        _, evidence = _evidence(applied)
        assert (
            evidence["desired_match"] is True and track.output_routing_type.attached_object is group
        )
        assert _literal_notes(clip) == notes and tuple(track._devices) == devices
        # Equal name/index does not transfer this private Group grant to a
        # replacement native object, even after a previous successful ACK.
        replacement = Track(workflow.live.song, group.name, "group")
        for ordinal in range(len(workflow.live.song.return_tracks)):
            replacement._mixer._add_send(f"Existing send {ordinal}")
        group_index = workflow.live.song.tracks.index(group)
        workflow.live.song._tracks[group_index] = replacement
        track._group_actual = replacement
        denied = _route(client, "preview", "adopt_group", **arguments)
        assert denied["success"] is False and "replacement" in denied["error"], denied
        assert len(workflow.native_route_calls) == 1 and workflow.native_return_calls == []


def test_explicit_new_group_association_retires_stale_private_cohort_not_history(routing_workflow):
    """Current A→B approval enables the exact selected member without reusing A's authority."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        first_id = _owning_group(client)
        first = _native_group(workflow, (track,), "First current native Group")
        first_args = _group_arguments(workflow, first_id, first, track)
        granted = _routing_apply(
            client,
            "adopt_group",
            _routing_preview(client, "adopt_group", **first_args),
            **first_args,
        )
        _evidence(granted, group=True)
        _refresh_clip(client)
        second_id = _owning_group(client, ())
        _ok(client, "assign_channel_to_group", graph_id=1, channel_id=1, group_id=second_id)
        second = _native_group(workflow, (track,), "Second current native Group")
        second_args = _group_arguments(workflow, second_id, second, track)
        before = _attempts(workflow)
        preview = _routing_preview(client, "adopt_group", **second_args)
        revised = _routing_apply(client, "adopt_group", preview, **second_args)
        _evidence(revised, group=True)
        _refresh_clip(client)
        retained = _ok(client, "project_realization_inspect", attempt_id=granted["attempt_id"])
        assert retained["receipt"] == granted["receipt"]
        assert len(_attempts(workflow)) > len(before) and track.group_track is second
        observed = _ok(client, "project_realization_inspect", score_id=1, part_id=1)
        proof = observed["native_observation"]["observation"]["group_authority"]
        assert proof["group_track_index"] == workflow.live.song.tracks.index(second)
        assert proof["approved_preview_fingerprint"] == preview["preview_fingerprint"]
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        assert workflow.native_route_calls == [] and workflow.native_return_calls == []


def test_lost_return_reply_blocks_other_project_and_namespace_then_queries_original(
    routing_workflow, monkeypatch
):
    """Unresolved Return history survives restart and namespace proposals without another append."""
    workflow = routing_workflow
    foreign_path = workflow.directory / "other-namespace.json"
    foreign_server_path = "/data/other-namespace.json" if workflow.command else str(foreign_path)
    with monkeypatch.context() as environment:
        environment.setenv("SUNNY_WORKSPACE_PATH", foreign_server_path)
        with workflow.process() as other:
            _ok(
                other,
                "score_create",
                title="Other namespace",
                total_bars=1,
                parts=[{"name": "Piano", "instrument_type": 47}],
            )
            _ok(other, "create_project", score_id=1)
            _ok(other, "workspace_save", path=foreign_server_path)
    with workflow.process() as client:
        _create_source(client, workflow)
        _second_project(client)
        aux = _aux(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        preview = _routing_preview(client, "create_return", aux_id=aux)
        workflow.drop_next_reply = APPLY_METHOD
        uncertain = _routing_apply(client, "create_return", preview, aux_id=aux)
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        before = _attempts(workflow)
        blocked = client.call(
            "project_realization_create",
            score_id=2,
            part_id=1,
            expected_project_revision=_revision(client, 2),
        )
        assert blocked["state"] == "set_wide_reconciliation_required", blocked
        assert (
            blocked["attempt_id"] == uncertain["attempt_id"] and not blocked["mutation_dispatched"]
        )
        changed = client.call("workspace_open", path=foreign_server_path)
        assert changed["success"] is False and "namespace switch" in changed["error"].lower(), (
            changed
        )
        assert uncertain["attempt_id"] in changed["error"]
        assert _revision(client, 2) > 0 and _attempts(workflow) == before
        assert len(workflow.live.song.return_tracks) == 2 and workflow.native_return_calls == [
            "create_return_track"
        ]
    with workflow.process() as restarted:
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["query_succeeded"] and reconciled["receipt"]["outcome"] == "acknowledged"
        assert not reconciled["mutation_retried"]
        created = restarted.call(
            "project_realization_create",
            score_id=2,
            part_id=1,
            expected_project_revision=_revision(restarted, 2),
        )
        assert created["success"], created
        assert (
            workflow.native_return_calls == ["create_return_track"]
            and len(workflow.live.song.return_tracks) == 2
        )
    assert workflow.managed_calls.count(APPLY_METHOD) == 1


def test_equal_valued_replaced_send_handle_after_preview_stops_native_append(routing_workflow):
    """An equal-valued replacement parameter changes native identity and conveys no authority."""
    workflow = routing_workflow
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        aux = _aux(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        preview = _routing_preview(client, "create_return", aux_id=aux)
        original = track.mixer_device.sends[0]
        replacement = copy.copy(original)
        track._mixer._sends = (replacement,)
        result = _routing_apply(client, "create_return", preview, aux_id=aux)
        assert result["success"] is False and result["history_saved"], result
        assert result["receipt"]["journal"]["native_mutation_started"] is False
        assert workflow.native_return_calls == [] and len(workflow.live.song.return_tracks) == 1
        assert replacement.value == original.value and replacement is not original
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
