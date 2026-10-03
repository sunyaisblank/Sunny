"""One deployment scenario that runs against the offline Live model and a real Live.

The scenario talks to sunny-mcp only through MCP tools, so it makes no
assumption about what sits behind the bridge. Against the offline model it
runs in every CI build (test_live_end_to_end.py); against a real Live it is
the final live check (test_live_host.py). Everything it learns about the
target is returned, so a live run also records the facts that issue #22
leaves open.
"""

from __future__ import annotations

from typing import Any, Protocol


class McpClient(Protocol):
    """Anything that calls a sunny-mcp tool and returns its decoded result."""

    def call(self, tool: str, **arguments: Any) -> dict[str, Any]:
        """Call one MCP tool."""
        ...


def _pitch(letter: str, octave: int) -> dict[str, Any]:
    return {"letter": letter, "accidental": 0, "octave": octave}


def _whole(n: int, d: int) -> dict[str, int]:
    return {"n": n, "d": d}


def run_live_smoke(client: McpClient) -> dict[str, Any]:
    """Deploy a small two-part project and verify it by readback.

    Preconditions: the Set may hold other tracks; the project inserts its own
    two tracks and leaves the rest unchanged. Postconditions asserted: the
    plan applies with every journal entry acknowledged, every requested note
    is written, the track count rises by exactly two, and the Remote Script
    log records no error. Returns the observations for the record.
    """
    before = client.call("get_ableton_session_state")
    assert before.get("success") is True, before
    log_start = client.call("get_ableton_remote_log")
    assert log_start.get("success") is True, log_start

    score = client.call(
        "score_create",
        title="Sunny live check",
        total_bars=1,
        time_sig_num=4,
        time_sig_den=4,
        bpm=96,
        parts=[
            {"name": "Sunny Chords", "instrument_type": 47},
            {"name": "Sunny Melody", "instrument_type": 47},
        ],
    )
    chords, melody = score["part_ids"]
    for letter in ("C", "E", "G"):
        inserted = client.call(
            "score_insert_note",
            score_id=score["score_id"],
            part_id=chords,
            bar=1,
            offset=_whole(0, 1),
            pitch=_pitch(letter, 4),
            duration=_whole(1, 1),
        )
        assert inserted.get("ok") is True, inserted
    # A triplet: three 1/12 notes form one 3:2 group over the first beat.
    line = [
        {
            "position": {"bar": 1, "beat_n": unit, "beat_d": 12},
            "pitch": _pitch(letter, 5),
            "duration": _whole(1, 12),
        }
        for unit, letter in enumerate("CDE")
    ]
    line.append(
        {
            "position": {"bar": 1, "beat_n": 1, "beat_d": 4},
            "pitch": _pitch("G", 5),
            "duration": _whole(3, 4),
        }
    )
    written = client.call(
        "score_write_melody", score_id=score["score_id"], part_id=melody, melody=line
    )
    assert written.get("ok") is True, written

    profiles = []
    for part_id, name in ((chords, "Chords"), (melody, "Melody")):
        profile = client.call("create_timbre_profile", part_id=part_id, name=name)
        source = client.call(
            "set_sound_source",
            profile_id=profile["profile_id"],
            source_type="fm",
            operator_count=4,
        )
        assert source.get("success") is True, source
        profiles.append(profile["profile_id"])
    mix = client.call("create_mix_graph", part_ids=[chords, melody])
    for channel in client.call("get_mix_json", graph_id=mix["graph_id"])["mix_ir"]["channels"]:
        level = client.call(
            "set_channel_level", graph_id=mix["graph_id"], channel_id=channel["id"], level_db=-6.0
        )
        assert level.get("success") is True, level

    project = {
        "score_id": score["score_id"],
        "timbre_profile_ids": profiles,
        "mix_graph_id": mix["graph_id"],
    }
    validation = client.call("project_validate", **project)
    assert validation["valid"] is True, validation
    plan = client.call("project_plan_to_ableton", **project)
    assert plan.get("success") is True, plan
    applied = client.call("project_apply_ableton_plan", plan_id=plan["plan_id"])
    assert applied.get("success") is True, applied
    deployment = applied["deployment"]
    assert deployment["status"] == "completed", deployment
    outcomes = {entry["outcome"] for entry in deployment["mutation_journal"]}
    assert outcomes == {"acknowledged"}, outcomes
    assert applied["score"]["notes_written"] == applied["score"]["notes_requested"] == 7

    after = client.call("get_ableton_session_state")
    assert after.get("success") is True, after
    assert after["track_count"] == before["track_count"] + 2, (before, after)

    log = client.call("get_ableton_remote_log", after_sequence=log_start["next_sequence"])
    assert log.get("success") is True, log
    errors = [entry for entry in log["entries"] if entry["level"] in ("ERROR", "CRITICAL")]
    assert not errors, errors

    return {
        "live_version": before.get("target_profile", {}).get("live", {}).get("version"),
        "tracks_before": before["track_count"],
        "tracks_after": after["track_count"],
        "planned_mutations": len(plan["planned_mutations"]),
        "warnings": {
            key: value.get("warnings")
            for key, value in applied.items()
            if isinstance(value, dict) and value.get("warnings")
        },
        "remote_log": log["entries"],
    }
