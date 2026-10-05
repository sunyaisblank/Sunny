"""Independent schema literals and native identity models, without a Live host."""

from __future__ import annotations

import copy
import socket
from types import SimpleNamespace

import pytest
from live_model import Clip, CuePoint, DeviceParameter, LiveSet, Track, inject
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.legacy import INTENT_KEYS, TOKEN_KEYS, valid_legacy_request
from Sunny.managed import ManagedRegistry, _canonical_bytes, _digest
from Sunny.native_control import PeerControl, peer_scope


@pytest.fixture
def target(monkeypatch, request):
    """Use actual authorizers on the source-observed model and an owned socket."""
    live = LiveSet(getattr(request, "param", (12, 3, 5)), midi_tracks=2, return_tracks=1).install(
        monkeypatch
    )
    registry = ManagedRegistry(live.surface)
    handler = LomHandler(live.surface, managed_registry=registry)
    registry.attach_handler(handler)
    left, right = socket.socketpair()
    peer = PeerControl(left)
    peer.phase("native")
    state = SimpleNamespace(live=live, registry=registry, handler=handler, peer=peer, remote=right)
    with peer_scope(peer):
        context = call(state, "sunny_managed_context")
        state.scope = {**context, "scope_id": None}
        scope = call(state, "sunny_legacy_scope", state.scope)
        assert scope["outcome"] == "ready", scope
        state.scope = {
            key: scope[key]
            for key in ("schema_version", "bridge_instance", "document_token", "scope_id")
        }
        state.revision = 0
        yield state
        registry._legacy.close()
    peer.close()
    left.close()
    right.close()


def call(target, name, value=None):
    """Require a valid native outer envelope; inner failure remains distinct."""
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": name,
            "args": [] if value is None else [value],
        }
    )
    assert response["success"], response
    return response["value"]


def intent(target, number=1, command=None):
    """Describe one immutable command in its original workflow and revision."""
    return {
        **target.scope,
        "workflow_id": "c" * 32,
        "operation_id": f"{number:032x}",
        "ordinal": number,
        "graph_revision": target.revision,
        "command": command or {"type": "set", "path": "song", "name": "tempo", "args": [140.0]},
    }


def token(request):
    """Retain original correlation fields for execute/query only."""
    return {
        **{key: value for key, value in request.items() if key != "command"},
        "fingerprint": _digest(request),
    }


def execute(target, request):
    """Prepare without setters, then execute once and retain its revision."""
    prepared = call(target, "sunny_legacy_prepare", request)
    assert prepared["outcome"] == "prepared", prepared
    result = call(target, "sunny_legacy_execute", token(request))
    if result["outcome"] == "acknowledged":
        target.revision = result["result"]["graph_revision"]
    return result


def test_literal_golden_sm1_and_closed_preparation(target):
    """Pin independently supplied bytes/types, not a self-generated expected hash."""
    literal = {
        "schema_version": 1,
        "bridge_instance": "b" * 32,
        "document_token": "d" * 32,
        "scope_id": "a" * 32,
        "workflow_id": "c" * 32,
        "operation_id": "e" * 32,
        "ordinal": 1,
        "graph_revision": 0,
        "command": {"type": "set", "path": "song", "name": "tempo", "args": [140.0]},
    }
    assert set(literal) == INTENT_KEYS
    assert len(_canonical_bytes(literal)) == 431
    assert _digest(literal) == "8e3e529fb67e726541d9c464e2a6a87208b4c7673b482805c21d15678e3e652c"
    integer = copy.deepcopy(literal)
    integer["command"]["args"] = [140]
    assert _digest(integer) != _digest(literal)
    assert valid_legacy_request("sunny_legacy_prepare", [literal])
    original = intent(target)
    prepared = call(target, "sunny_legacy_prepare", original)
    assert set(prepared) == TOKEN_KEYS | {
        "outcome",
        "native_mutation_started",
        "started_calls",
        "returned_calls",
        "result",
        "diagnostic",
        "error",
    }
    assert prepared["started_calls"] == 0 and not prepared["native_mutation_started"]
    assert target.live.song.tempo == 120.0
    acknowledged = call(target, "sunny_legacy_execute", token(original))
    assert acknowledged["outcome"] == "acknowledged"
    assert acknowledged["started_calls"] == acknowledged["returned_calls"] == 1
    assert acknowledged["result"]["value"] == {
        "property": "tempo",
        "requested": 140.0,
        "observed": 140.0,
    }
    assert call(target, "sunny_legacy_execute", token(original)) == acknowledged
    assert call(target, "sunny_legacy_operation", token(original)) == acknowledged


@pytest.mark.parametrize(
    "change",
    [
        lambda value: value.update(extra=True),
        lambda value: value.pop("command"),
        lambda value: value.update(schema_version=True),
        lambda value: value.update(schema_version=1.0),
        lambda value: value.update(ordinal=0),
        lambda value: value.update(ordinal=4097),
        lambda value: value.update(graph_revision=False),
        lambda value: value.update(scope_id="A" * 32),
        lambda value: value["command"].update(bridge_protocol_version=47),
        lambda value: value["command"].update(name="sunny_legacy_execute"),
        lambda value: value["command"].update(name="sunny_managed_context"),
        lambda value: value["command"].update(path="song//"),
    ],
)
def test_malformed_closed_intents_do_not_reserve_or_mutate(target, change):
    """Malformed literals never consume shared operation IDs or setter phases."""
    request = intent(target)
    change(request)
    assert not valid_legacy_request("sunny_legacy_prepare", [request])
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_legacy_prepare",
            "args": [request],
        }
    )
    assert not response["success"]
    assert not target.registry._operations
    assert target.live.song.tempo == 120.0


def test_original_writable_value_drift_is_not_recaptured(target):
    """Planning before-state cannot silently refresh at preparation."""
    target.live.song.tempo = 135.0
    journal = call(target, "sunny_legacy_prepare", intent(target))
    assert journal["outcome"] == "declined" and journal["started_calls"] == 0
    assert target.live.song.tempo == 135.0


def test_same_named_track_substitution_and_reorder_decline(target):
    """Equal labels cannot substitute a retained native target."""
    song = target.live.song
    original = song._tracks[0]
    song._tracks[0] = Track(song, original.name, "midi")
    request = intent(
        target, command={"type": "set", "path": "song/tracks/0", "name": "mute", "args": [True]}
    )
    result = call(target, "sunny_legacy_prepare", request)
    assert result["outcome"] == "declined"
    assert original.mute is False and song._tracks[0].mute is False


def test_dense_workflow_requires_previous_acknowledgment(target):
    """A missing predecessor ACK poisons continuation without a fresh workflow."""
    first = intent(target)
    assert call(target, "sunny_legacy_prepare", first)["outcome"] == "prepared"
    second = intent(target, 2)
    assert call(target, "sunny_legacy_prepare", second)["outcome"] == "declined"
    assert call(target, "sunny_legacy_execute", token(first))["outcome"] == "declined"
    assert target.live.song.tempo == 120.0


def test_finish_releases_handles_but_retains_original_declined_journal(target):
    """Closing authority releases all graph references while preserving tokens."""
    request = intent(target)
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    assert call(target, "sunny_legacy_finish", target.scope)["outcome"] == "closed"
    record = target.registry._operations[request["operation_id"]]
    assert not {"scope", "target", "parameter", "peer", "extra"} & set(record)
    assert target.registry._legacy.used_nodes == 0
    assert call(target, "sunny_legacy_operation", token(request))["outcome"] == "declined"
    assert call(target, "sunny_legacy_execute", token(request))["started_calls"] == 0


@pytest.mark.parametrize(
    "method,args", [("create_midi_track", [0]), ("create_scene", [0]), ("create_return_track", [])]
)
def test_owned_creation_preserves_old_identity_and_continues(target, method, args):
    """Exact structural transitions preserve old objects and advance one revision."""
    old_tracks = tuple(target.live.song.tracks)
    old_slots = tuple(old_tracks[0].clip_slots)
    result = execute(
        target,
        intent(target, command={"type": "call", "path": "song", "name": method, "args": args}),
    )
    assert result["outcome"] == "acknowledged", result
    assert target.revision == 1
    if method == "create_midi_track":
        assert tuple(target.live.song.tracks)[1:] == old_tracks
    elif method == "create_scene":
        assert tuple(old_tracks[0].clip_slots)[1:] == old_slots
    else:
        assert len(old_tracks[0].mixer_device.sends) == 2
    next_request = intent(
        target, 2, {"type": "set", "path": "song/tracks/0", "name": "name", "args": ["Owned"]}
    )
    assert execute(target, next_request)["outcome"] == "acknowledged"


def test_clip_creation_then_notes_clear_and_delete(target):
    """Fresh Score Clip creation remains usable under mandatory authority."""
    slot_path = "song/tracks/0/clip_slots/0"
    assert (
        execute(
            target,
            intent(
                target,
                command={"type": "call", "path": slot_path, "name": "create_clip", "args": [4.0]},
            ),
        )["outcome"]
        == "acknowledged"
    )
    notes = [
        {
            "pitch": 60,
            "start_time": 0.0,
            "duration": 1.0,
            "velocity": 100,
            "mute": False,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0,
        }
    ]
    result = execute(
        target,
        intent(
            target,
            2,
            {
                "type": "call",
                "path": slot_path + "/clip",
                "name": "add_new_notes",
                "args": [{"notes": notes}],
            },
        ),
    )
    assert result["outcome"] == "acknowledged", result
    assert (
        execute(
            target,
            intent(
                target,
                3,
                {
                    "type": "call",
                    "path": slot_path + "/clip",
                    "name": "sunny_clear_all_envelopes",
                    "args": [],
                },
            ),
        )["outcome"]
        == "acknowledged"
    )
    assert (
        execute(
            target,
            intent(
                target, 4, {"type": "call", "path": slot_path, "name": "delete_clip", "args": []}
            ),
        )["outcome"]
        == "acknowledged"
    )
    assert target.live.song.tracks[0].clip_slots[0].has_clip is False


def test_stopped_cue_has_four_native_phases_and_original_playhead(target):
    """Stopped cue creation journals each call and restores its original playhead."""
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [8.0, "Section"],
            },
        ),
    )
    assert result["outcome"] == "acknowledged", result
    assert result["started_calls"] == result["returned_calls"] == 4
    assert target.live.song.current_song_time == 0.0
    assert target.live.song.cue_points[0].name == "Section"
    assert result["diagnostic"]["created"][0]["kind"] == "cue"
    assert call(target, "sunny_legacy_scope", target.scope)["outcome"] == "ready"


def test_scoped_reads_and_pure_doctor_reads_keep_distinct_authority(target):
    """Read values use the original graph; diagnostics do not capture another Set."""
    authority = target.registry._legacy
    count = len(authority.scopes)
    for name in (
        "sunny_get_target_profile",
        "sunny_get_scene_count",
        "sunny_get_track_count",
        "sunny_get_return_track_count",
    ):
        call(target, name)
    assert len(authority.scopes) == count
    observed = call(
        target,
        "sunny_legacy_read",
        {
            **target.scope,
            "graph_revision": 0,
            "command": {"type": "get", "path": "song", "name": "tempo", "args": []},
        },
    )
    assert observed["outcome"] == "observed" and observed["value"] == 120.0
    target.live.song._tracks.reverse()
    declined = call(
        target,
        "sunny_legacy_read",
        {
            **target.scope,
            "graph_revision": 0,
            "command": {"type": "get", "path": "song", "name": "tempo", "args": []},
        },
    )
    assert declined["outcome"] == "declined" and declined["value"] is None


def test_instrument_parameter_and_routing_use_owned_handles(target):
    """An owned instrument transition admits real parameter and routing continuation."""
    path = "song/tracks/0"
    inserted = execute(
        target,
        intent(
            target,
            command={"type": "call", "path": path, "name": "insert_device", "args": ["Drift"]},
        ),
    )
    assert inserted["outcome"] == "acknowledged", inserted
    assert target.revision == 1
    device_path = path + "/devices/0"
    result = execute(
        target,
        intent(
            target,
            2,
            {
                "type": "call",
                "path": device_path,
                "name": "sunny_set_device_parameter",
                "args": ["Device On", 1.0, "value", 0.0, 1.0],
            },
        ),
    )
    assert result["outcome"] == "acknowledged", result
    track = target.live.song.tracks[0]
    routes = target.handler._output_routing_snapshot(track)
    external = next(
        item
        for item in routes["available_output_routing_types"]["available_output_routing_types"]
        if item["display_name"] == "Ext. Out"
    )
    result = execute(
        target,
        intent(
            target,
            3,
            {
                "type": "call",
                "path": path,
                "name": "sunny_set_output_routing_type",
                "args": [external],
            },
        ),
    )
    assert result["outcome"] == "acknowledged", result
    assert result["result"]["graph_revision"] == 1
    assert call(target, "sunny_legacy_scope", target.scope)["outcome"] == "ready"


@pytest.mark.parametrize(
    "failure", ["raises_after_effect", "malformed_readback", "oversized_readback"]
)
def test_started_effect_keeps_bounded_original_partial_without_replay(target, monkeypatch, failure):
    """Independent host failures preserve phase evidence and never repeat a setter."""
    descriptor = type(target.live.song).tempo
    original = descriptor._setter
    entered = []

    def native(song, value):
        entered.append(value)
        original(song, value)
        if failure == "raises_after_effect":
            raise RuntimeError("provider raised after setting tempo")
        inject(song, "tempo", True if failure == "malformed_readback" else "x" * 50000)

    monkeypatch.setattr(descriptor, "_setter", native)
    request = intent(target)
    result = execute(target, request)
    assert result["outcome"] == "partial" and result["native_mutation_started"]
    assert result["started_calls"] == 1
    assert result["returned_calls"] == (0 if failure == "raises_after_effect" else 1)
    assert result["result"] is None and len(result["error"].encode()) <= 2048
    assert call(target, "sunny_legacy_execute", token(request)) == result
    assert call(target, "sunny_legacy_operation", token(request)) == result
    assert entered == [140.0]


def test_reservation_failure_is_before_native_entry(target, monkeypatch):
    """An exhausted shared retained-byte budget refuses before a setter or ID reservation."""
    from Sunny import legacy

    authority = target.registry._legacy
    monkeypatch.setattr(legacy, "MAX_BYTES", authority.used_bytes + 100)
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_legacy_prepare",
            "args": [intent(target)],
        }
    )
    assert not response["success"]
    assert not target.registry._operations and target.live.song.tempo == 120.0


def test_shared_family_collision_never_creates_legacy_effect_authority(target):
    """An ordinary/managed operation ID cannot be reinterpreted as a generic intent."""
    request = intent(target)
    target.registry._operations[request["operation_id"]] = {"ordinary": True}
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_legacy_prepare",
            "args": [request],
        }
    )
    assert not response["success"]
    assert target.live.song.tempo == 120.0


def test_fresh_peer_may_query_but_never_execute_prepared_original(target):
    """Reconnect is original-token inspection without replacement effect authority."""
    request = intent(target)
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    left, right = socket.socketpair()
    peer = PeerControl(left)
    peer.phase("native")
    try:
        with peer_scope(peer):
            assert call(target, "sunny_legacy_operation", token(request))["outcome"] == "prepared"
            declined = call(target, "sunny_legacy_execute", token(request))
            assert declined["outcome"] == "declined" and declined["started_calls"] == 0
            assert target.live.song.tempo == 120.0
    finally:
        peer.close()
        left.close()
        right.close()


def test_new_document_reports_original_unknown_epoch_without_rebinding(target):
    """Value-equal Set replacement gives unavailable original evidence, not fresh authority."""
    request = intent(target)
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    replacement = LiveSet(midi_tracks=2, return_tracks=1)
    target.live.surface._song = replacement.song  # Surface properties return a fresh wrapper.
    target.registry._surface._song = replacement.song
    result = call(target, "sunny_legacy_operation", token(request))
    assert result["outcome"] == "unknown_epoch"
    assert result["bridge_instance"] == request["bridge_instance"]
    assert result["document_token"] == request["document_token"]
    assert result["result"] is None and result["error"] is None
    assert replacement.song.tempo == 120.0


def test_revocation_between_cue_phases_stops_without_compensation(target, monkeypatch):
    """A disconnected playhead phase cannot toggle or restore the original Song."""
    descriptor = type(target.live.song).current_song_time
    original = descriptor._setter
    entered = []

    def move(song, value):
        entered.append(value)
        original(song, value)
        target.remote.close()

    monkeypatch.setattr(descriptor, "_setter", move)
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [8.0, "Section"],
            },
        ),
    )
    assert result["outcome"] == "partial"
    assert result["started_calls"] == result["returned_calls"] == 1
    assert entered == [8.0] and target.live.song.current_song_time == 8.0
    assert target.live.song.cue_points == ()


def test_graph_accounting_and_identity_reservation_precede_creation(target, monkeypatch):
    """Exact settled bytes release reservations and refuse exhausted nodes before creation."""
    from Sunny import legacy

    authority = target.registry._legacy
    assert execute(target, intent(target))["outcome"] == "acknowledged"
    graph = authority.scopes[target.scope["scope_id"]]["graph"]
    assert graph.previous is None
    expected = graph.encoded_bytes + sum(
        record["budget"] for record in target.registry._operations.values()
    )
    assert authority.used_bytes == expected and authority.reserved_nodes == 0
    monkeypatch.setattr(legacy, "MAX_NODES", authority.used_nodes + 1)
    request = intent(
        target, 2, {"type": "call", "path": "song", "name": "create_midi_track", "args": [-1]}
    )
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_legacy_prepare",
            "args": [request],
        }
    )
    assert not response["success"] and len(target.live.song.tracks) == 2


@pytest.mark.parametrize("method", ["delete_clip", "sunny_clear_all_envelopes"])
def test_existing_opaque_envelopes_decline_without_destructive_entry(target, method):
    """Presence or sampled values never establish complete existing envelope content."""
    slot_path = "song/tracks/0/clip_slots/0"
    assert (
        execute(
            target,
            intent(
                target,
                command={"type": "call", "path": slot_path, "name": "create_clip", "args": [4.0]},
            ),
        )["outcome"]
        == "acknowledged"
    )
    clip = target.live.song.tracks[0].clip_slots[0].clip
    clip._envelopes.add("unobserved native content")
    # Capture a deliberately new original scope that observes presence, yet
    # cannot turn it into complete destructive content authority.
    captured = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
    assert captured["outcome"] == "ready"
    target.scope["scope_id"], target.revision = captured["scope_id"], 0
    command = {
        "type": "call",
        "path": slot_path if method == "delete_clip" else slot_path + "/clip",
        "name": method,
        "args": [],
    }
    request = intent(target, command=command)
    request["operation_id"] = "9" * 32
    result = call(target, "sunny_legacy_prepare", request)
    assert result["outcome"] == "declined" and result["started_calls"] == 0
    assert clip.has_envelopes is True and target.live.song.tracks[0].clip_slots[0].clip is clip


def test_protocol47_raw_setter_gate_does_not_resolve_or_mutate(target, monkeypatch):
    """Pin the synchronized admission transition without weakening production WIP46."""
    from Sunny import handler

    monkeypatch.setattr(handler, "BRIDGE_PROTOCOL_VERSION", 47)
    entered = []
    monkeypatch.setattr(target.handler, "_resolve_path", lambda path: entered.append(path))
    response = target.handler.handle(
        {
            "bridge_protocol_version": 47,
            "type": "set",
            "path": "song",
            "name": "tempo",
            "args": [140.0],
        }
    )
    assert response == {
        "success": False,
        "error": "Raw native mutation requires prepared scope authority",
    }
    assert entered == [] and target.live.song.tempo == 120.0


def test_unrelated_audio_clip_identity_does_not_invoke_midi_population_api(target, monkeypatch):
    """Mixed Set capture retains Audio Clip identity without inventing audio-content authority."""
    slot = target.live.song.tracks[0].clip_slots[0]
    slot.create_clip(4.0)
    clip = slot.clip
    inject(clip, "is_midi_clip", False)
    inject(clip, "is_audio_clip", True)
    monkeypatch.setattr(
        clip, "get_all_notes_extended", lambda: pytest.fail("MIDI API read on Audio Clip")
    )
    captured = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
    assert captured["outcome"] == "ready", captured
    target.scope["scope_id"] = captured["scope_id"]
    assert execute(target, intent(target))["outcome"] == "acknowledged"
    deletion = call(
        target,
        "sunny_legacy_prepare",
        intent(
            target,
            2,
            {
                "type": "call",
                "path": "song/tracks/0/clip_slots/0",
                "name": "delete_clip",
                "args": [],
            },
        ),
    )
    assert deletion["outcome"] == "declined" and deletion["started_calls"] == 0
    assert slot.clip is clip


def test_note_change_event_cannot_be_erased_by_equal_restored_values(target):
    """An event indicating intervening edits invalidates original authority even after reversion."""
    slot_path = "song/tracks/0/clip_slots/0"
    assert (
        execute(
            target,
            intent(
                target,
                command={"type": "call", "path": slot_path, "name": "create_clip", "args": [4.0]},
            ),
        )["outcome"]
        == "acknowledged"
    )
    clip = target.live.song.tracks[0].clip_slots[0].clip
    assert tuple(clip.get_all_notes_extended()) == ()

    clip._notify_notes_changed()
    result = call(
        target,
        "sunny_legacy_prepare",
        intent(
            target,
            2,
            {
                "type": "call",
                "path": slot_path + "/clip",
                "name": "sunny_clear_all_envelopes",
                "args": [],
            },
        ),
    )
    assert result["outcome"] == "declined" and result["started_calls"] == 0
    assert tuple(clip.get_all_notes_extended()) == ()


def test_disconnect_during_final_before_getter_declines_before_setter(target, monkeypatch):
    """A returned getter cannot authorize setter entry after that peer has left."""
    request = intent(target)
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    graph = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    original = graph.check_before
    calls = []

    def disconnect_after_observation(node, fields=None):
        original(node, fields)
        if node["kind"] == "song":
            calls.append(node)
            if len(calls) == 2:  # execute admission, then the final phase before-state
                target.remote.close()

    monkeypatch.setattr(graph, "check_before", disconnect_after_observation)
    result = call(target, "sunny_legacy_execute", token(request))
    assert result["outcome"] == "declined"
    assert result["native_mutation_started"] is False
    assert result["started_calls"] == result["returned_calls"] == 0
    assert target.live.song.tempo == 120.0


@pytest.mark.parametrize(
    "parameter,field,first,second",
    [
        ("volume", "display_value", -6.0, -3.0),
        ("panning", "value", 0.25, 0.75),
        ("track_activator", "value", 0.0, 1.0),
        ("sends/0", "display_value", -12.0, -6.0),
    ],
)
def test_owned_parameter_coupled_readback_permits_second_original_write(
    target, parameter, field, first, second
):
    """Actual model writes couple value/display; only the original target is refreshed."""
    path = "song/tracks/0/mixer_device/" + parameter
    for ordinal, value in enumerate((first, second), 1):
        result = execute(
            target,
            intent(target, ordinal, {"type": "set", "path": path, "name": field, "args": [value]}),
        )
        assert result["outcome"] == "acknowledged", result
    unrelated = target.live.song.tracks[1].mixer_device.volume
    unrelated.display_value = -5.0
    result = call(
        target,
        "sunny_legacy_prepare",
        intent(
            target,
            3,
            {
                "type": "set",
                "path": "song/tracks/1/mixer_device/volume",
                "name": "display_value",
                "args": [-3.0],
            },
        ),
    )
    assert result["outcome"] == "declined" and result["started_calls"] == 0


@pytest.mark.parametrize("capacity", ["nodes", "bytes", "one_node", "field_bytes"])
def test_combined_capture_capacity_stops_before_unreserved_target_getters(
    target, monkeypatch, capacity
):
    """A second scope cannot allocate first and discover the shared bound afterwards."""
    from Sunny import legacy

    authority = target.registry._legacy
    observed = []
    descriptor = type(target.live.song).tempo if capacity != "one_node" else Track.name
    original = descriptor._getter
    monkeypatch.setattr(descriptor, "_getter", lambda obj: observed.append(obj) or original(obj))
    if capacity in {"bytes", "field_bytes"}:
        room = (
            1
            if capacity == "bytes"
            else len(legacy._bytes({"binding": "0" * 32, "path": "song", "kind": "song"}))
        )
        monkeypatch.setattr(legacy, "MAX_BYTES", authority.used_bytes + room)
    else:
        monkeypatch.setattr(
            legacy, "MAX_NODES", authority.used_nodes + (1 if capacity == "one_node" else 0)
        )
    used = (authority.used_nodes, authority.used_bytes, len(authority.scopes))
    result = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
    assert result["outcome"] == "declined"
    assert observed == []
    assert (authority.used_nodes, authority.used_bytes, len(authority.scopes)) == used


def test_structural_candidate_shares_original_baselines_and_uses_reserved_delta(target):
    """Verified creation does not duplicate the already charged before-state graph."""
    authority = target.registry._legacy
    graph = authority.scopes[target.scope["scope_id"]]["graph"]
    node = graph.node("song/tracks/0/mixer_device/volume")
    result = execute(
        target,
        intent(
            target, command={"type": "call", "path": "song", "name": "create_scene", "args": [-1]}
        ),
    )
    assert result["outcome"] == "acknowledged", result
    candidate = authority.scopes[target.scope["scope_id"]]["graph"]
    assert candidate.node("song/tracks/0/mixer_device/volume") is node
    assert candidate.previous is None
    assert authority.used_nodes == candidate.identity_count
    assert authority.used_bytes == candidate.encoded_size() + sum(
        record["budget"] for record in target.registry._operations.values() if record.get("legacy")
    )


def test_candidate_observation_does_not_erase_original_note_change_event(target, monkeypatch):
    """The old listener stays authoritative until the replacement graph is committed."""
    slot_path = "song/tracks/0/clip_slots/0"
    assert (
        execute(
            target,
            intent(
                target,
                command={"type": "call", "path": slot_path, "name": "create_clip", "args": [4.0]},
            ),
        )["outcome"]
        == "acknowledged"
    )
    clip = target.live.song.tracks[0].clip_slots[0].clip
    original = clip.get_all_notes_extended

    def observation_with_event():
        result = original()
        clip._notify_notes_changed()
        return result

    monkeypatch.setattr(clip, "get_all_notes_extended", observation_with_event)
    result = execute(
        target,
        intent(target, 2, {"type": "call", "path": "song", "name": "create_scene", "args": [-1]}),
    )
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == result["returned_calls"] == 1
    assert clip is target.live.song.tracks[0].clip_slots[0].clip


def test_unexpected_device_population_stops_before_unreserved_parameter_getters(
    target, monkeypatch
):
    """An oversized actual creation keeps Partial evidence without capturing more authority."""
    original = Track.insert_device
    native = []
    parameter_reads = []

    def insert(track, *args):
        original(track, *args)
        native.append(track)
        device = next(member for member in track.devices if member is not track.mixer_device)
        parameters = tuple(DeviceParameter(f"new {index}") for index in range(2000))
        inject(device, "parameters", parameters)

    monkeypatch.setattr(Track, "insert_device", insert)
    monkeypatch.setattr(
        DeviceParameter.name,
        "_getter",
        lambda parameter: parameter_reads.append(parameter) or parameter._name,
    )
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song/tracks/0",
                "name": "insert_device",
                "args": ["Drift"],
            },
        ),
    )
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == result["returned_calls"] == 1
    assert len(native) == 1 and parameter_reads == []


def test_device_parameter_getter_drift_declines_immediately_before_setter(target, monkeypatch):
    """Helper readback cannot replace the retained parameter's original before-state."""
    assert (
        execute(
            target,
            intent(
                target,
                command={
                    "type": "call",
                    "path": "song/tracks/0",
                    "name": "insert_device",
                    "args": ["Drift"],
                },
            ),
        )["outcome"]
        == "acknowledged"
    )
    track = target.live.song.tracks[0]
    device = next(member for member in track.devices if member is not track.mixer_device)
    parameter = next(
        parameter for parameter in device.parameters if parameter.name == "Filter Freq"
    )
    wanted = intent(
        target,
        2,
        {
            "type": "call",
            "path": "song/tracks/0/devices/0",
            "name": "sunny_set_device_parameter",
            "args": ["Filter Freq", 0.2, "value", 0.0, 1.0],
        },
    )
    assert call(target, "sunny_legacy_prepare", wanted)["outcome"] == "prepared"
    descriptor = DeviceParameter.default_value
    original = descriptor._getter
    setters = []
    original_write = DeviceParameter.value._setter

    def drift_from_helper(obj):
        if obj is parameter:
            obj._value = 0.8
        return original(obj)

    monkeypatch.setattr(descriptor, "_getter", drift_from_helper)
    monkeypatch.setattr(
        DeviceParameter.value,
        "_setter",
        lambda obj, value: setters.append((obj, value)) or original_write(obj, value),
    )
    result = call(target, "sunny_legacy_execute", token(wanted))
    assert result["outcome"] == "declined", result
    assert result["started_calls"] == result["returned_calls"] == 0
    assert setters == [] and parameter.value == 0.8


def _fresh_scope(target):
    call(target, "sunny_legacy_finish", target.scope)
    captured = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
    assert captured["outcome"] == "ready", captured
    target.scope["scope_id"], target.revision = captured["scope_id"], 0


@pytest.mark.parametrize("running", [False, True])
def test_guarded_new_cue_records_exact_four_phases_without_start_stop(target, monkeypatch, running):
    """Running P0 freezes at execute; this model supplies no actual Live timing proof."""
    song = target.live.song
    song.set_transport_flag("is_playing", running)
    _fresh_scope(target)
    request = intent(
        target,
        command={
            "type": "call",
            "path": "song",
            "name": "sunny_set_cue",
            "args": [12.0, "Section"],
        },
    )
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    if running:
        song._current_song_time = 7.0  # naturally advanced after planning/preparation
    initial = song.current_song_time
    entered = []
    move, toggle = type(song).current_song_time._setter, type(song).set_or_delete_cue
    monkeypatch.setattr(
        type(song).current_song_time,
        "_setter",
        lambda obj, value: entered.append(value) or move(obj, value),
    )
    monkeypatch.setattr(
        type(song), "set_or_delete_cue", lambda obj: entered.append("toggle") or toggle(obj)
    )
    result = call(target, "sunny_legacy_execute", token(request))
    assert result["outcome"] == "acknowledged", result
    assert result["started_calls"] == result["returned_calls"] == 4
    assert entered == [12.0, "toggle", initial]
    assert song.current_song_time == initial and song.is_playing is running
    assert song.cue_points[0].time == 12.0 and song.cue_points[0].name == "Section"


@pytest.mark.parametrize("boundary", [1, 2, 3])
@pytest.mark.parametrize("running", [False, True])
@pytest.mark.parametrize(
    "drift", ["is_playing", "session_record", "record_mode", "playhead", "peer"]
)
def test_cue_boundary_drift_keeps_original_partial_and_stops_forward_phases(
    target, monkeypatch, boundary, drift, running
):
    """Each returned phase may reenter; no later restoration or rename is compensation."""
    song = target.live.song
    song.set_transport_flag("is_playing", running)
    song._current_song_time = 7.0
    _fresh_scope(target)
    entered = []
    move, toggle = type(song).current_song_time._setter, type(song).set_or_delete_cue

    def after():
        if len(entered) == boundary:
            if drift == "peer":
                target.remote.close()
            elif drift == "playhead":
                song._current_song_time += 0.5
            else:
                song.set_transport_flag(drift, not running if drift == "is_playing" else True)

    def write(obj, value):
        entered.append(value)
        move(obj, value)
        after()

    def create(obj):
        entered.append("toggle")
        toggle(obj)
        after()

    monkeypatch.setattr(type(song).current_song_time, "_setter", write)
    monkeypatch.setattr(type(song), "set_or_delete_cue", create)
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [12.0, "Section"],
            },
        ),
    )
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == result["returned_calls"] == boundary
    assert entered == [12.0, "toggle", 7.0][:boundary]
    if song.cue_points:
        assert song.cue_points[0].name != "Section"


def test_wrong_time_created_cue_is_not_restored_or_renamed(target, monkeypatch):
    """A novel handle alone is insufficient: its time must match before restoration."""
    song = target.live.song
    toggle = type(song).set_or_delete_cue

    def create_elsewhere(obj):
        toggle(obj)
        obj._cue_points[-1]._time += 1.0

    monkeypatch.setattr(type(song), "set_or_delete_cue", create_elsewhere)
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [12.0, "Section"],
            },
        ),
    )
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == result["returned_calls"] == 2
    assert song.current_song_time == 12.0
    assert song.cue_points[0].time == 13.0 and song.cue_points[0].name != "Section"


@pytest.mark.parametrize("field", ["name", "time"])
def test_existing_cue_before_state_drift_is_rechecked_before_rename(target, monkeypatch, field):
    """A native getter changing the retained cue cannot authorize a fresh name write."""
    song = target.live.song
    song.set_or_delete_cue()
    cue = song.cue_points[0]
    _fresh_scope(target)
    request = intent(
        target,
        command={"type": "call", "path": "song", "name": "sunny_set_cue", "args": [0.0, "Section"]},
    )
    assert call(target, "sunny_legacy_prepare", request)["outcome"] == "prepared"
    descriptor = CuePoint.name
    original = descriptor._getter
    altered = []

    def edit_after_read(obj):
        actual = original(obj)
        if obj is cue and not altered:
            altered.append(obj)
            if field == "name":
                obj._name = "External edit"
            else:
                obj._time = 2.0
        return actual

    monkeypatch.setattr(descriptor, "_getter", edit_after_read)
    setters = []
    monkeypatch.setattr(descriptor, "_setter", lambda obj, value: setters.append(value))
    result = call(target, "sunny_legacy_execute", token(request))
    assert result["outcome"] == "declined", result
    assert result["started_calls"] == result["returned_calls"] == 0
    assert altered == [cue] and setters == []


@pytest.mark.parametrize("boundary", [1, 2, 3, 4])
def test_cue_setter_exception_keeps_started_count_and_never_compensates(
    target, monkeypatch, boundary
):
    """An effect may occur before a call raises; restoration never runs from finally."""
    song = target.live.song
    entered = []
    move, toggle, rename = (
        type(song).current_song_time._setter,
        type(song).set_or_delete_cue,
        CuePoint.name._setter,
    )

    def native(callback, *args):
        entered.append(callback)
        callback(*args)
        if len(entered) == boundary:
            raise RuntimeError("Native raised after effect")

    monkeypatch.setattr(
        type(song).current_song_time, "_setter", lambda obj, value: native(move, obj, value)
    )
    monkeypatch.setattr(type(song), "set_or_delete_cue", lambda obj: native(toggle, obj))
    monkeypatch.setattr(CuePoint.name, "_setter", lambda obj, value: native(rename, obj, value))
    request = intent(
        target,
        command={
            "type": "call",
            "path": "song",
            "name": "sunny_set_cue",
            "args": [12.0, "Section"],
        },
    )
    result = execute(target, request)
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == boundary and result["returned_calls"] == boundary - 1
    assert len(entered) == boundary

    assert call(target, "sunny_legacy_execute", token(request)) == result
    assert call(target, "sunny_legacy_operation", token(request)) == result
    assert len(entered) == boundary


@pytest.mark.parametrize("running", [False, True])
@pytest.mark.parametrize("boundary", [1, 2, 3])
@pytest.mark.parametrize("field", ["name", "time"])
def test_unrelated_original_cue_scalar_drift_stops_creation_phases(
    target, monkeypatch, running, boundary, field
):
    """An original cue edited by a returned setter never becomes refreshed authority."""
    song = target.live.song
    song.set_or_delete_cue()
    original_cue = song.cue_points[0]
    original_cue.name = "Original"
    song.set_transport_flag("is_playing", running)
    _fresh_scope(target)
    entered = []
    move, toggle = type(song).current_song_time._setter, type(song).set_or_delete_cue

    def after():
        if len(entered) == boundary:
            if field == "name":
                original_cue._name = "External edit"
            else:
                original_cue._time = 2.0

    def write(obj, value):
        entered.append(value)
        move(obj, value)
        after()

    def create(obj):
        entered.append("toggle")
        toggle(obj)
        after()

    monkeypatch.setattr(type(song).current_song_time, "_setter", write)
    monkeypatch.setattr(type(song), "set_or_delete_cue", create)
    request = intent(
        target,
        command={
            "type": "call",
            "path": "song",
            "name": "sunny_set_cue",
            "args": [12.0, "Section"],
        },
    )
    result = execute(target, request)
    assert result["outcome"] == "partial", result
    assert result["started_calls"] == result["returned_calls"] == boundary
    assert entered == [12.0, "toggle", 0.0][:boundary]
    assert call(target, "sunny_legacy_operation", token(request)) == result
    assert call(target, "sunny_legacy_execute", token(request)) == result
    assert len(entered) == boundary


@pytest.mark.parametrize("other_drift", [False, True])
def test_existing_cue_rename_excludes_only_its_owned_target_from_after_guard(
    target, monkeypatch, other_drift
):
    """The intended one-phase rename remains valid while another old cue stays protected."""
    song = target.live.song
    song.set_or_delete_cue()
    target_cue = song.cue_points[0]
    song.current_song_time = 4.0
    song.set_or_delete_cue()
    other = next(cue for cue in song.cue_points if cue is not target_cue)
    _fresh_scope(target)
    rename = CuePoint.name._setter

    def write(obj, value):
        rename(obj, value)
        if obj is target_cue and other_drift:
            other._name = "External edit"

    monkeypatch.setattr(CuePoint.name, "_setter", write)
    result = execute(
        target,
        intent(
            target,
            command={
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [0.0, "Section"],
            },
        ),
    )
    assert result["outcome"] == ("partial" if other_drift else "acknowledged"), result
    assert result["started_calls"] == result["returned_calls"] == 1
    assert target_cue.name == "Section" and song.current_song_time == 4.0


def _note(pitch=60, start=0.0, duration=1.0):
    """Supply exact independent note literals, including a duration crossing the range end."""
    return {
        "pitch": pitch,
        "start_time": start,
        "duration": duration,
        "velocity": 100,
        "mute": False,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0,
    }


def _original_clip(target):
    """Seed original native content before a deliberately new planning scope."""
    slot = target.live.song.tracks[0].clip_slots[0]
    slot.create_clip(4.0)
    clip = slot.clip
    clip.add_new_notes(
        tuple(
            target.live.module.Clip.MidiNoteSpecification(**note)
            for note in (_note(0), _note(127, 3.75), _note(62, 4.0), _note(64, 8.0))
        )
    )
    return clip


@pytest.mark.parametrize("target", [(11, 0, 0), (11, 1, 0)], indirect=True)
def test_versioned_scope_retains_original_clip_and_explicit_note_boundary(target, monkeypatch):
    """An exposed 11.0 stub cannot authorize full notes; unknown notes do not block graph work."""
    clip = _original_clip(target)
    original_getattribute, all_notes = Clip.__getattribute__, Clip.get_all_notes_extended
    full_calls = []

    def exposed(obj, name):
        if name == "get_all_notes_extended":
            return object.__getattribute__(obj, name)
        return original_getattribute(obj, name)

    def versioned_full(obj):
        full_calls.append(obj)
        if obj._live_version < (11, 1):
            raise RuntimeError("get_all_notes_extended requires Live 11.1")
        return all_notes(obj)

    monkeypatch.setattr(Clip, "__getattribute__", exposed)
    monkeypatch.setattr(Clip, "get_all_notes_extended", versioned_full)
    assert callable(clip.get_all_notes_extended)
    _fresh_scope(target)
    graph = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    path = "song/tracks/0/clip_slots/0/clip"
    original_node = graph.node(path)
    legacy = target.live.application.version_tuple() < (11, 1)
    assert original_node["object"] is clip
    assert original_node["note_boundary"] == {
        "entire_population": not legacy,
        "time_span": 4.0 if legacy else None,
        "owned_creation": False,
    }
    assert [note["note_id"] for note in original_node["before"]["notes"]] == (
        [1, 2] if legacy else [1, 2, 3, 4]
    )
    assert graph.encoded_bytes == graph.encoded_size()
    assert execute(target, intent(target))["outcome"] == "acknowledged"
    created = execute(
        target,
        intent(
            target,
            2,
            {"type": "call", "path": "song", "name": "create_midi_track", "args": [0]},
        ),
    )
    assert created["outcome"] == "acknowledged", created
    candidate = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    assert candidate.node("song/tracks/1/clip_slots/0/clip") is original_node
    assert tuple(note.note_id for note in clip.get_notes_by_id((1, 2, 3, 4))) == (1, 2, 3, 4)
    if legacy:
        assert full_calls == []
    else:
        assert full_calls


@pytest.mark.parametrize("target", [(11, 0, 0)], indirect=True)
def test_live11_owned_clip_finite_insertions_and_empty_envelope_clear_continue(target):
    """Fresh authoring proves IDs and its range while leaving whole-population evidence false."""
    original = _original_clip(target)
    _fresh_scope(target)
    slot_path = "song/tracks/1/clip_slots/0"
    create = intent(
        target,
        command={"type": "call", "path": slot_path, "name": "create_clip", "args": [4.0]},
    )
    assert execute(target, create)["outcome"] == "acknowledged"
    clear = intent(
        target,
        2,
        {
            "type": "call",
            "path": slot_path + "/clip",
            "name": "sunny_clear_all_envelopes",
            "args": [],
        },
    )
    assert execute(target, clear)["outcome"] == "acknowledged"
    clip = target.live.song.tracks[1].clip_slots[0].clip
    for ordinal, note in ((3, _note(0)), (4, _note(127, 3.75))):
        request = intent(
            target,
            ordinal,
            {
                "type": "call",
                "path": slot_path + "/clip",
                "name": "add_new_notes",
                "args": [{"notes": [note]}],
            },
        )
        result = execute(target, request)
        assert result["outcome"] == "acknowledged", result
        assert result["started_calls"] == result["returned_calls"] == 1
        assert result["result"]["value"] == [ordinal - 2]
        assert call(target, "sunny_legacy_operation", token(request)) == result
        assert call(target, "sunny_legacy_execute", token(request)) == result
    assert tuple(note.note_id for note in clip.get_notes_by_id((1, 2))) == (1, 2)
    assert tuple(note.note_id for note in original.get_notes_by_id((1, 2, 3, 4))) == (1, 2, 3, 4)
    graph = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    node = graph.node(slot_path + "/clip")
    assert node["note_boundary"] == {
        "entire_population": False,
        "time_span": 4.0,
        "owned_creation": True,
    }
    assert [note["note_id"] for note in node["before"]["notes"]] == [1, 2]
    assert graph.encoded_bytes == graph.encoded_size()


@pytest.mark.parametrize("target", [(11, 0, 0)], indirect=True)
@pytest.mark.parametrize("method", ["delete_clip", "sunny_clear_all_envelopes"])
def test_live11_original_empty_range_does_not_authorize_destructive_content(target, method):
    """An empty range cannot erase an original Clip containing only invisible notes."""
    slot_path = "song/tracks/0/clip_slots/0"
    slot = target.live.song.tracks[0].clip_slots[0]
    slot.create_clip(4.0)
    clip = slot.clip
    clip.add_new_notes((target.live.module.Clip.MidiNoteSpecification(**_note(64, 8.0)),))
    _fresh_scope(target)
    graph = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    assert graph.node(slot_path + "/clip")["before"]["notes"] == []
    request = intent(
        target,
        command={
            "type": "call",
            "path": slot_path if method == "delete_clip" else slot_path + "/clip",
            "name": method,
            "args": [],
        },
    )
    declined = call(target, "sunny_legacy_prepare", request)
    assert declined["outcome"] == "declined"
    assert declined["started_calls"] == declined["returned_calls"] == 0
    assert "Entire original Clip note population is unavailable" in declined["error"]
    assert slot.clip is clip and clip.get_notes_by_id((1,))[0].start_time == 8.0


@pytest.mark.parametrize("target", [(11, 0, 0)], indirect=True)
@pytest.mark.parametrize("at_capture", [False, True])
def test_live11_outside_range_listener_event_is_sticky_before_any_effect(
    target, monkeypatch, at_capture
):
    """An outside-range edit and undo invalidates both capture and later native admission."""
    clip = _original_clip(target)

    def edit_and_undo():
        note = clip.get_notes_by_id((4,))[0]
        note.velocity = 33.0
        clip._notify_notes_changed()
        note.velocity = 100.0
        clip._notify_notes_changed()

    if at_capture:
        original = clip.get_notes_extended

        def during_read(**kwargs):
            result = original(**kwargs)
            edit_and_undo()
            return result

        monkeypatch.setattr(clip, "get_notes_extended", during_read)
        call(target, "sunny_legacy_finish", target.scope)
        captured = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
        assert captured["outcome"] == "declined"
        assert not clip._note_listeners
    else:
        _fresh_scope(target)
        edit_and_undo()
        declined = call(target, "sunny_legacy_prepare", intent(target))
        assert declined["outcome"] == "declined" and declined["started_calls"] == 0
    assert clip.get_notes_by_id((4,))[0].velocity == 100.0
    assert target.live.song.tempo == 120.0


@pytest.mark.parametrize("target", [(11, 0, 0)], indirect=True)
def test_live11_fixed_original_note_range_does_not_expand_with_owned_markers(target):
    """Changing a marker is an owned scalar edit, not new whole-population authority."""
    clip = _original_clip(target)
    _fresh_scope(target)
    clip_path = "song/tracks/0/clip_slots/0/clip"
    result = execute(
        target,
        intent(
            target, command={"type": "set", "path": clip_path, "name": "end_marker", "args": [8.0]}
        ),
    )
    assert result["outcome"] == "acknowledged", result
    graph = target.registry._legacy.scopes[target.scope["scope_id"]]["graph"]
    node = graph.node(clip_path)
    assert node["note_boundary"]["time_span"] == 4.0 and clip.end_marker == 8.0
    assert [note["note_id"] for note in graph.observe_notes(node)] == [1, 2]
    outside = intent(
        target,
        2,
        {
            "type": "call",
            "path": clip_path,
            "name": "add_new_notes",
            "args": [{"notes": [_note(60, 4.0)]}],
        },
    )
    declined = call(target, "sunny_legacy_prepare", outside)
    assert declined["outcome"] == "declined" and declined["started_calls"] == 0


@pytest.mark.parametrize("target", [(11, 0, 0)], indirect=True)
def test_live11_inconsistent_id_readback_is_partial_and_never_replayed(target, monkeypatch):
    """A real returned insertion followed by missing-ID proof stays indeterminate."""
    clip = _original_clip(target)
    _fresh_scope(target)
    original_add, original_by_id = clip.add_new_notes, clip.get_notes_by_id
    effects = []

    def insert(specifications):
        effects.append(tuple(specifications))
        return original_add(specifications)

    monkeypatch.setattr(clip, "add_new_notes", insert)
    monkeypatch.setattr(clip, "get_notes_by_id", lambda identities: original_by_id(()))
    request = intent(
        target,
        command={
            "type": "call",
            "path": "song/tracks/0/clip_slots/0/clip",
            "name": "add_new_notes",
            "args": [{"notes": [_note(60, 1.0)]}],
        },
    )
    result = execute(target, request)
    assert result["outcome"] == "partial"
    assert result["started_calls"] == result["returned_calls"] == 1
    assert call(target, "sunny_legacy_operation", token(request)) == result
    assert call(target, "sunny_legacy_execute", token(request)) == result
    assert len(effects) == 1 and original_by_id((5,))[0].start_time == 1.0


@pytest.mark.parametrize("target", [(11, 1, 0)], indirect=True)
def test_supported_full_population_error_never_falls_back_to_permissive_range(target, monkeypatch):
    """Only an observed API floor selects legacy access; host getter failure stays a decline."""
    clip = _original_clip(target)

    def full_error():
        raise RuntimeError("Native complete population getter failed")

    monkeypatch.setattr(clip, "get_all_notes_extended", full_error)
    monkeypatch.setattr(clip, "get_notes_extended", lambda **_kwargs: pytest.fail("range fallback"))
    call(target, "sunny_legacy_finish", target.scope)
    captured = call(target, "sunny_legacy_scope", {**target.scope, "scope_id": None})
    assert captured["outcome"] == "declined"
    assert "Native complete population getter failed" in captured["error"]
    assert not clip._note_listeners


@pytest.mark.parametrize("target", [(11, 0, 0), (11, 1, 0)], indirect=True)
@pytest.mark.parametrize("readback", ["population", "ids"])
def test_insert_readback_listener_drift_cannot_be_erased_as_the_owned_event(
    target, monkeypatch, readback
):
    """A changed and restored original note during getters cannot yield a stale ACK."""
    clip = _original_clip(target)
    _fresh_scope(target)
    original_by_id = clip.get_notes_by_id
    method = (
        "get_notes_by_id"
        if readback == "ids"
        else "get_notes_extended"
        if target.live.application.version_tuple() < (11, 1)
        else "get_all_notes_extended"
    )
    original_read = getattr(clip, method)

    def drift(*args, **kwargs):
        result = original_read(*args, **kwargs)
        old_note = original_by_id((1,))[0]
        old_note.velocity = 33.0
        clip._notify_notes_changed()
        old_note.velocity = 100.0
        clip._notify_notes_changed()
        return result

    monkeypatch.setattr(clip, method, drift)
    request = intent(
        target,
        command={
            "type": "call",
            "path": "song/tracks/0/clip_slots/0/clip",
            "name": "add_new_notes",
            "args": [{"notes": [_note(60, 1.0)]}],
        },
    )
    result = execute(target, request)
    assert result["outcome"] == "partial"
    assert result["started_calls"] == result["returned_calls"] == 1
    assert "Native note population changed during readback" in result["error"]
    assert call(target, "sunny_legacy_operation", token(request)) == result
    assert call(target, "sunny_legacy_execute", token(request)) == result
    assert original_by_id((1,))[0].velocity == 100.0
    assert tuple(note.note_id for note in original_by_id((1, 2, 3, 4, 5))) == (1, 2, 3, 4, 5)
