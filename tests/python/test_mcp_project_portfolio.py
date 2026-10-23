"""One finite owning-project portfolio; external provider evidence is not host proof."""

from __future__ import annotations

import base64
import copy
import io
import json
import xml.etree.ElementTree as ET
from fractions import Fraction

import mido
import pytest
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import (
    _assert_native_effect_values,
    _effect_selections,
    _effects,
    effect_workflow,
)
from test_mcp_native_envelope_revision_workflow import _approved, _preview
from test_mcp_realization_workflow import _literal_notes, _revision
from test_mcp_workspace_workflow import WorkspaceProcess, _author_triplet
from test_mcp_workspace_workflow import _call as _ok

INITIAL_NOTES = {
    1: [(60, 0.0, 2 / 3, 80.0), (64, 2 / 3, 1 / 3, 80.0), (69, 4.0, 1.0, 76.0)],
    2: [(55, 0.0, 5.0, 72.0)],
}
COMBINED_NOTES = {
    1: [
        (62, 0.0, 2 / 3, 80.0),
        (66, 2 / 3, 1 / 3, 80.0),
        (69, 4.0, 1.0, 76.0),
        (62, 8.0, 1.0, 74.0),
    ],
    2: [(55, 0.0, 5.0, 72.0)],
}


def events(client, part, bar):
    """Read actual authoring IDs, never infer them from note positions or ordinals."""
    score = _ok(client, "score_get_json", score_id=1)
    selected = next(value for value in score["parts"] if value["id"] == part)
    return [
        event
        for event in selected["measures"][bar - 1]["voices"][0]["events"]
        if event["type"] == "note_group"
    ]


def insert(client, part, bar, letter, octave, duration, velocity, offset=(0, 1)):
    """Use literal concert pitch and exact whole-note allocations through the public tool."""
    _ok(
        client,
        "score_insert_note",
        score_id=1,
        part_id=part,
        bar=bar,
        offset={"n": offset[0], "d": offset[1]},
        pitch={"letter": letter, "accidental": 0, "octave": octave},
        duration={"n": duration[0], "d": duration[1]},
        velocity=velocity,
    )


def author_two_parts(client):
    """Existing public tools author a Bb triplet phrase and independent cross-bar hold."""
    _author_triplet(client, bars=2)
    added = _ok(client, "score_add_part", score_id=1, name="Held Piano", instrument_type=47)
    assert added["part_id"] == 2
    assert added["timbre_profile_id"] == 2 and added["channel_id"] == 2
    insert(client, 1, 2, "A", 4, (1, 4), 76)
    insert(client, 2, 1, "G", 3, (1, 1), 72)
    insert(client, 2, 2, "G", 3, (1, 4), 72)
    _ok(client, "score_set_tie", score_id=1, event_id=events(client, 2, 1)[0]["id"], tied=True)
    _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-6.0)
    _ok(client, "set_channel_pan", graph_id=1, channel_id=1, pan=-0.25)
    _ok(client, "set_channel_level", graph_id=1, channel_id=2, level_db=-6.0)
    _ok(client, "set_channel_pan", graph_id=1, channel_id=2, pan=0.25)
    return {part: events(client, part, 1)[0]["id"] for part in (1, 2)}


def source_for(client, profile):
    """The same four explicit Drift bindings apply to the selected actual owning profile."""
    if profile == 1:
        return _source(client)
    _ok(
        client,
        "set_sound_source",
        profile_id=profile,
        source_type="subtractive",
        filter_cutoff=1200.0,
        attack=250.0,
        decay=250.0,
        sustain=0.4,
        release=1000.0,
    )
    controls = [
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
    ]
    for path, name, _, low, high in controls:
        _ok(
            client,
            "map_timbre_parameter",
            profile_id=profile,
            ir_path=path,
            device_index=0,
            parameter_name=name,
            device_name="Drift",
            source_min=low,
            source_max=high,
            target_min=0.0,
            target_max=1.0,
            value_property="display_value",
        )
    return [
        {"source_path": path, "capability_id": capability, "tolerance": 0.05}
        for path, _, capability, _, _ in controls
    ]


def native_part(client, part, tool="project_realization_create", **extra):
    """Use the real public facade with current owning revision and explicit selected Part."""
    return client.call(
        tool, score_id=1, part_id=part, expected_project_revision=_revision(client), **extra
    )


def assert_notes(clip, expected):
    """Literal expected values never call production projection, tie, parser or plan helpers."""
    assert [
        (pitch, start, duration, velocity)
        for _, pitch, start, duration, velocity in _literal_notes(clip)
    ] == expected


def author_pan(client, revised=False):
    """Step values use exact whole-note offsets; last interval derives from actual Score extent."""
    if revised:
        _ok(client, "remove_mix_automation", graph_id=1, index=0)
    values = ((0, 1, -0.25), (1, 4, 0.25)) if revised else ((0, 1, -0.5), (1, 2, 0.5))
    _ok(
        client,
        "add_mix_automation",
        graph_id=1,
        target="channels[1].spatial.pan",
        interpolation=0,
        breakpoints=[{"bar": 1, "beat_num": n, "beat_den": d, "value": v} for n, d, v in values],
    )


def edit_notes_and_extent_together(client):
    """Root coordinator fixture: ONE final authored revision, no temporary Score rollback."""
    _ok(client, "score_insert_measures", score_id=1, after_bar=2, count=1)
    _ok(
        client,
        "score_transpose",
        score_id=1,
        region={"start_bar": 1, "end_bar": 1, "parts": [1]},
        interval={"chromatic": 2, "diatonic": 1},
    )
    insert(client, 1, 3, "D", 4, (1, 4), 74)
    return {"expected_clip_end": 12.0, "expected_notes": copy.deepcopy(COMBINED_NOTES)}


def add_then_remove_authored_part(client):
    """Future coordinator consumes real project lifecycle; retired native state is preserved."""
    added = _ok(client, "score_add_part", score_id=1, name="Added F", instrument_type=47)
    assert added["part_id"] == 3 and added["channel_id"] == 3
    insert(client, 3, 1, "F", 4, (1, 4), 78)
    _ok(client, "score_remove_part", score_id=1, part_id=2)
    actual = _ok(client, "get_project_json", score_id=1)
    assert [part["id"] for part in actual["documents"]["score"]["parts"]] == [1, 3]
    assert [channel["part_id"] for channel in actual["documents"]["mix"]["channels"]] == [1, 3]
    return {"new_part_id": 3, "retired_part_id": 2, "new_notes": [(65, 0.0, 1.0, 78.0)]}


def test_two_part_symbolic_exports_save_and_restart(tmp_path, monkeypatch):
    """Independently parsed tied/transposing exports and authoring IDs survive workspace restart."""
    workspace = WorkspaceProcess(tmp_path, monkeypatch)
    with workspace.start() as client:
        heads = author_two_parts(client)
        exported = _ok(client, "score_export_midi", score_id=1, ppq=480)["midi_base64"]
        midi = mido.MidiFile(file=io.BytesIO(base64.b64decode(exported, validate=True)))
        tick, actual = 0, {0: [], 1: []}
        for message in midi.tracks[0]:
            tick += message.time
            if message.type in ("note_on", "note_off"):
                actual[message.channel].append((tick, message.type, message.note))
        assert actual == {
            0: [
                (0, "note_on", 60),
                (320, "note_off", 60),
                (320, "note_on", 64),
                (480, "note_off", 64),
                (1920, "note_on", 69),
                (2400, "note_off", 69),
            ],
            1: [(0, "note_on", 55), (2400, "note_off", 55)],
        }
        xml = ET.fromstring(_ok(client, "score_compile_to_musicxml", score_id=1)["xml"])
        first = xml.findall("part")[0]
        spelled = first.findall("measure/note[pitch]")
        assert [
            (
                note.findtext("pitch/step"),
                note.findtext("pitch/alter", "0"),
                note.findtext("pitch/octave"),
            )
            for note in spelled
        ] == [("D", "0", "4"), ("D", "0", "4"), ("F", "1", "4"), ("B", "0", "4")]
        divisions = int(first.findtext("measure/attributes/divisions"))
        assert [Fraction(int(note.findtext("duration")), 4 * divisions) for note in spelled] == [
            Fraction(1, 12),
            Fraction(1, 12),
            Fraction(1, 12),
            Fraction(1, 4),
        ]
        _ok(client, "workspace_save", path=workspace.path)
    with workspace.start() as restarted:
        assert _ok(restarted, "score_export_midi", score_id=1, ppq=480)["midi_base64"] == exported
        assert {part: events(restarted, part, 1)[0]["id"] for part in (1, 2)} == heads
        prospective = edit_notes_and_extent_together(restarted)
        assert prospective["expected_clip_end"] == 12.0
        lifecycle = add_then_remove_authored_part(restarted)
        assert lifecycle["retired_part_id"] == 2


@pytest.fixture
def portfolio_native(tmp_path, monkeypatch):
    """Reuse the existing actual MCP/TCP effect provider, not a new host simulator."""
    provider = effect_workflow.__wrapped__(tmp_path, monkeypatch)
    workflow = next(provider)
    try:
        yield workflow
    finally:
        provider.close()


def test_existing_public_native_phases_and_lost_reply(portfolio_native):
    """Run existing native phases; Root connects the pending coordinator, Mixer and Return steps."""
    workflow = portfolio_native
    with workflow.process() as client:
        author_two_parts(client)
        sources = {part: source_for(client, part) for part in (1, 2)}
        selections, _, _ = _effect_selections(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        for part in (1, 2):
            created = native_part(client, part)
            assert created["success"] and created["history_saved"], created
            source = native_part(
                client, part, "project_realization_author_timbre", selections=sources[part]
            )
            assert source["success"], source
        tracks = {part: workflow.live.song.tracks[part] for part in (1, 2)}
        clips = {part: tracks[part].clip_slots[0].clip for part in (1, 2)}
        ids = {part: tuple(clips[part]._notes) for part in (1, 2)}
        for part in (1, 2):
            assert_notes(clips[part], INITIAL_NOTES[part])
            assert clips[part].end_marker == 8.0
            clips[part]._user_mpe_expression = {ids[part][0]: {"pressure": [0.1, 0.8]}}
            clips[part]._user_follow_actions = {"next": True}
        effects = _effects(client, selections)
        assert effects["success"], effects
        _assert_native_effect_values(tracks[1])
        author_pan(client)
        lane = native_part(client, 1, "project_realization_author_mix_lane", lane_index=0)
        assert lane["success"] and lane["native_lane_samples_verified"], lane
        envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert [envelope.value_at_time(time) for time in (0, 1, 2, 5, 7)] == [
            -0.5,
            -0.5,
            0.5,
            0.5,
            0.5,
        ]
        author_pan(client, revised=True)
        preview = _preview(client)
        assert preview["success"], preview
        replaced = _approved(client, preview["preview"])
        assert replaced["success"], replaced
        envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert [envelope.value_at_time(time) for time in (0, 0.5, 1, 4, 7)] == [
            -0.25,
            -0.25,
            0.25,
            0.25,
            0.25,
        ]
        _ok(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2450.0)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_update_device_parameters"
        lost = _timbre(client, sources[1])
        assert lost["success"] is False and lost["receipt"]["outcome"] == "indeterminate", lost
        assert tracks[1]._devices[0].parameters[1].value == 0.75
        original_token = lost["attempt_id"]
        setters_before = workflow.managed_calls.count("sunny_managed_update_device_parameters")
    with workflow.process() as restarted:
        reconciled = _ok(restarted, "project_realization_reconcile", attempt_id=original_token)
        assert (
            reconciled["receipt"]["outcome"] == "acknowledged"
            and not reconciled["mutation_retried"]
        )
        assert (
            workflow.managed_calls.count("sunny_managed_update_device_parameters") == setters_before
        )
        for part in (1, 2):
            assert workflow.live.song.tracks[part] is tracks[part]
            assert tracks[part].clip_slots[0].clip is clips[part]
            assert tuple(clips[part]._notes) == ids[part]
            assert_notes(clips[part], INITIAL_NOTES[part])
            assert clips[part]._user_mpe_expression == {ids[part][0]: {"pressure": [0.1, 0.8]}}
            assert clips[part]._user_follow_actions == {"next": True}
        saved = json.loads(workflow.workspace.read_text())
        assert saved["native_realization"]["workspace_namespace"]
        assert (
            _ok(restarted, "project_realization_inspect", attempt_id=original_token)[
                "retry_authorized"
            ]
            is False
        )
