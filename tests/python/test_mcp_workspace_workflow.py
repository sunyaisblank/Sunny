"""Author, export, save, restart and recover through the actual stdio server.

SUNNY_WORKSPACE_MCP_COMMAND selects the same portfolio through a container with
{workspace_dir} mounted at /data; no Live connection is configured here.
"""

from __future__ import annotations

import base64
import io
import json
import os
import shlex
import subprocess
import xml.etree.ElementTree as ET
from contextlib import contextmanager
from fractions import Fraction
from pathlib import Path

import mido
import pytest
from test_live_end_to_end import _McpClient, _sunny_mcp_binary


class WorkspaceProcess:
    """Restart a server against one durable directory with an explicit offline configuration."""

    def __init__(self, directory, monkeypatch):
        """Choose a native path or the mounted container path."""
        self.directory = directory
        self.monkeypatch = monkeypatch
        configured = os.environ.get("SUNNY_WORKSPACE_MCP_COMMAND")
        self.command = (
            [token.format(workspace_dir=str(directory)) for token in shlex.split(configured)]
            if configured
            else [str(_sunny_mcp_binary())]
        )
        if configured:
            directory.chmod(0o777)  # The image's sunny user owns newly created files.
        self.path = (
            "/data/workspace.sunny.json" if configured else str(directory / "workspace.json")
        )
        self.main = directory / Path(self.path).name
        monkeypatch.setenv("SUNNY_WORKSPACE_PATH", self.path)
        monkeypatch.delenv("SUNNY_WORKSPACE_RECOVERY", raising=False)
        monkeypatch.delenv("SUNNY_ABLETON_HOST", raising=False)
        monkeypatch.delenv("SUNNY_TCP_PORT", raising=False)

    @contextmanager
    def start(self):
        """Yield a real client; EOF stops the process without an implicit save."""
        client = _McpClient(None, 0, host=None, command=self.command)
        try:
            yield client
        finally:
            client.close()

    def rejected_start(self):
        """Capture the actual failed-start exit and stderr without starting a client thread."""
        return subprocess.run(
            self.command, input="", capture_output=True, text=True, timeout=40, check=False
        )


@pytest.fixture
def workspace_process(tmp_path, monkeypatch):
    """Use a fresh durable directory for each native/container workflow."""
    return WorkspaceProcess(tmp_path, monkeypatch)


def _call(client, tool, **arguments):
    result = client.call(tool, **arguments)
    assert "error" not in result, (tool, result)
    assert result.get("success") is not False, (tool, result)
    return result


def _pitch(letter, octave=4):
    return {"letter": letter, "accidental": 0, "octave": octave}


def _position(bar, numerator=0, denominator=1):
    return {"bar": bar, "beat_n": numerator, "beat_d": denominator}


def _author_triplet(client, bars=1, extra_parts=False):
    parts = [{"name": "Clarinet", "instrument_type": 16}]
    if extra_parts:
        parts += [
            {"name": "Violin", "instrument_type": 0},
            {"name": "Cello", "instrument_type": 2},
        ]
    _call(client, "score_create", title="Durable phrase", total_bars=bars, parts=parts)
    _call(client, "create_project", score_id=1)
    for index, letter in enumerate(("C", "C", "E")):
        _call(
            client,
            "score_insert_note",
            score_id=1,
            part_id=1,
            bar=1,
            offset={"n": index, "d": 12},
            pitch=_pitch(letter),
            duration={"n": 1, "d": 12},
            velocity=80,
        )
    score = _call(client, "score_get_json", score_id=1)
    events = score["parts"][0]["measures"][0]["voices"][0]["events"]
    notes = [event for event in events if event["type"] == "note_group"]
    assert len(notes) == 3
    _call(client, "score_set_tie", score_id=1, event_id=notes[0]["id"], tied=True)
    # Exact insertion already supplies the required rational tuplet context.
    # Exercise its explicit remove/recreate lifecycle without double scaling.
    contexts = [event["tuplet_context"] for event in notes]
    assert len({context["id"] for context in contexts}) == 1
    assert all(context["actual"] == 3 and context["normal"] == 2 for context in contexts)
    _call(client, "score_remove_tuplet_group", score_id=1, tuplet_id=contexts[0]["id"])
    _call(
        client,
        "score_create_tuplet_group",
        score_id=1,
        event_ids=[event["id"] for event in notes],
        actual=3,
        normal=2,
        normal_type={"n": 1, "d": 8},
        scaled_allocation={"n": 1, "d": 4},
    )


def test_tied_transposing_tuplet_exports_are_independently_parsed(workspace_process):
    """Mido and ElementTree agree with hand-derived sound, written pitch and exact allocations."""
    with workspace_process.start() as client:
        _author_triplet(client)
        exported = _call(client, "score_export_midi", score_id=1, ppq=480)
        midi = mido.MidiFile(
            file=io.BytesIO(base64.b64decode(exported["midi_base64"], validate=True))
        )
        assert midi.type == 0 and midi.ticks_per_beat == 480 and len(midi.tracks) == 1
        tick = 0
        sounding = []
        for message in midi.tracks[0]:
            tick += message.time
            if message.type in ("note_on", "note_off"):
                sounding.append(
                    (tick, message.type, message.note, message.channel, message.velocity)
                )
        assert [(time, kind, note, channel) for time, kind, note, channel, _ in sounding] == [
            (0, "note_on", 60, 0),
            (320, "note_off", 60, 0),
            (320, "note_on", 64, 0),
            (480, "note_off", 64, 0),
        ]
        xml = ET.fromstring(_call(client, "score_compile_to_musicxml", score_id=1)["xml"])
        divisions = int(xml.findtext("part/measure/attributes/divisions"))
        pitches = xml.findall("part/measure/note[pitch]")
        assert [note.findtext("pitch/step") for note in pitches] == ["D", "D", "F"]
        assert [note.findtext("pitch/alter", "0") for note in pitches] == ["0", "0", "1"]
        assert [int(note.findtext("pitch/octave")) for note in pitches] == [4, 4, 4]
        assert [Fraction(int(note.findtext("duration")), divisions * 4) for note in pitches] == [
            Fraction(1, 12)
        ] * 3
        assert xml.findtext("part/measure/attributes/transpose/chromatic") == "-2"
        assert xml.findtext("part/measure/attributes/transpose/diatonic") == "-1"
        assert [[tie.attrib["type"] for tie in note.findall("tie")] for note in pitches] == [
            ["start"],
            ["stop"],
            [],
        ]
        for note in pitches:
            assert note.findtext("time-modification/actual-notes") == "3"
            assert note.findtext("time-modification/normal-notes") == "2"
        assert "error" in client.call("score_export_midi", score_id=1, ppq=32768)


def test_lilypond_parser_preserves_written_tied_tuplet(workspace_process, tmp_path):
    """The actual 2.24 parser accepts the export and preserves hand-derived written timing."""
    configured = os.environ.get("SUNNY_LILYPOND_COMMAND")
    if not configured:
        pytest.skip("SUNNY_LILYPOND_COMMAND selects an actual LilyPond 2.24 parser")
    with workspace_process.start() as client:
        _author_triplet(client)
        source = _call(client, "score_compile_to_lilypond", score_id=1)["ly"]
    # The product exports notation. Add a MIDI block only for the independent
    # parser's timing oracle; its pitches follow the written transposition.
    assert source.count("\\layout { }") == 1
    (tmp_path / "phrase.ly").write_text(
        source.replace("\\layout { }", "\\layout { }\n  \\midi { }"), encoding="utf-8"
    )
    command = [token.format(workspace_dir=str(tmp_path)) for token in shlex.split(configured)]
    parsed = subprocess.run(
        [*command, "--output=phrase", "phrase.ly"],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    assert parsed.returncode == 0, parsed.stdout + parsed.stderr
    assert (tmp_path / "phrase.pdf").is_file()
    midi = mido.MidiFile(tmp_path / "phrase.midi")
    tick = 0
    written = []
    for message in mido.merge_tracks(midi.tracks):
        tick += message.time
        if message.type in ("note_on", "note_off"):
            kind = "off" if message.type == "note_off" or message.velocity == 0 else "on"
            written.append((Fraction(tick, midi.ticks_per_beat), kind, message.note))
    assert written == [
        (Fraction(0), "on", 62),
        (Fraction(2, 3), "off", 62),
        (Fraction(2, 3), "on", 66),
        (Fraction(1), "off", 66),
    ]


def test_whole_authoring_workspace_restarts_and_remains_revisable(workspace_process):
    """A multipart project, shared presets and analysed corpus survive an actual server restart."""
    with workspace_process.start() as client:
        _author_triplet(client, bars=16, extra_parts=True)
        for bar in range(2, 17):
            for part, pitch in ((1, _pitch("E")), (2, _pitch("G")), (3, _pitch("C", 3))):
                _call(
                    client,
                    "score_insert_note",
                    score_id=1,
                    part_id=part,
                    bar=bar,
                    pitch=pitch,
                    duration={"n": 1, "d": 2},
                    velocity=72,
                )
        _call(
            client,
            "score_set_key_signature",
            score_id=1,
            position=_position(9),
            root=_pitch("G"),
            mode="major",
        )
        _call(
            client,
            "score_set_tempo_map",
            score_id=1,
            entries=[
                {
                    "position": _position(1),
                    "bpm": {"n": 120, "d": 1},
                    "beat_unit": "quarter",
                    "transition": "immediate",
                },
                {
                    "position": _position(9),
                    "bpm": {"n": 185, "d": 2},
                    "beat_unit": "quarter",
                    "transition": "immediate",
                },
            ],
        )
        _call(
            client,
            "score_insert_hairpin",
            score_id=1,
            part_id=1,
            start=_position(2),
            end=_position(8),
            type=0,
        )
        _call(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=4200)
        _call(client, "save_preset", profile_id=1, name="Clarinet contour")
        _call(
            client,
            "add_automation",
            profile_id=1,
            path="source.filter.cutoff",
            breakpoints=[{"bar": 1, "value": 1800}, {"bar": 17, "value": 4200}],
        )
        _call(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-9)
        _call(client, "create_composer_profile", name="Reference author")
        xml = _call(client, "score_compile_to_musicxml", score_id=1)["xml"]
        # The bounded corpus XML reader explicitly rejects transposition state.
        # Its rejection must preserve the corpus; SMF supplies the sounding reference.
        before_ingestion = _call(client, "get_corpus_json")
        assert "error" in client.call(
            "ingest_musicxml", musicxml=xml, title="Reference phrase", composer_id=1
        )
        assert _call(client, "get_corpus_json") == before_ingestion
        midi = _call(client, "score_export_midi", score_id=1, ppq=480)
        _call(
            client,
            "ingest_midi",
            midi_base64=midi["midi_base64"],
            title="Reference phrase",
            composer_id=1,
            quantise_grid=48,
        )
        original = _call(client, "get_project_json", score_id=1)["documents"]
        corpus = _call(client, "get_corpus_json")
        saved = _call(client, "workspace_save", path=workspace_process.path)
        assert saved["committed"] is True
    with workspace_process.start() as restarted:
        assert _call(restarted, "get_project_json", score_id=1)["documents"] == original
        assert _call(restarted, "get_corpus_json") == corpus
        _call(restarted, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2300)
        assert (
            _call(restarted, "get_parameter", profile_id=1, path="source.filter.cutoff")["value"]
            == 2300
        )
        _call(restarted, "score_undo", score_id=1)
        assert (
            _call(restarted, "get_parameter", profile_id=1, path="source.filter.cutoff")["value"]
            == 4200
        )
        _call(restarted, "workspace_save", path=workspace_process.path)
    assert json.loads(workspace_process.main.read_text())["format"] == "sunny-workspace"


def test_corrupt_startup_requires_explicit_backup_recovery(workspace_process):
    """Corruption cannot silently create a blank workspace or overwrite its valid backup."""
    with workspace_process.start() as client:
        _author_triplet(client)
        _call(client, "workspace_save", path=workspace_process.path)
        _call(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2300)
        _call(client, "workspace_save", path=workspace_process.path)
    backup = Path(str(workspace_process.main) + ".bak")
    backup_bytes = backup.read_bytes()
    workspace_process.main.write_text('{"format":"sunny-workspace",')
    refused = workspace_process.rejected_start()
    assert refused.returncode != 0
    assert refused.stdout == ""
    assert "Workspace startup open failed" in refused.stderr
    assert backup.read_bytes() == backup_bytes
    workspace_process.monkeypatch.setenv("SUNNY_WORKSPACE_RECOVERY", "backup")
    with workspace_process.start() as recovered:
        _call(recovered, "get_project_json", score_id=1)
        assert workspace_process.main.read_text() == '{"format":"sunny-workspace",'
        _call(recovered, "workspace_save", path=workspace_process.path)
    assert backup.read_bytes() == backup_bytes
    workspace_process.monkeypatch.delenv("SUNNY_WORKSPACE_RECOVERY")
    with workspace_process.start() as reopened:
        _call(reopened, "get_project_json", score_id=1)
