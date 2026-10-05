"""Isolated released bridge/container witness around the external Live model."""

from __future__ import annotations

import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path


def main():
    """Use actual exported modules and production JSON without host Live or SSH."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", type=Path, required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--container", required=True)
    parser.add_argument("--owner", required=True)
    args = parser.parse_args()
    # This process has not imported Sunny. Its first package resolution must
    # select the exact native export, never the worktree's pending sources.
    sys.path.insert(0, str(args.release / "native"))
    import pytest
    from live_model import LiveSet
    from Sunny.build_identity import BRIDGE_SOURCE_SHA256
    from Sunny.surface import SunnyControlSurface
    from test_live_end_to_end import _LiveMainThread, _McpClient

    assert (
        Path(sys.modules["Sunny.surface"].__file__)
        .resolve()
        .is_relative_to((args.release / "native").resolve())
    )
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    configuration = args.directory / "paired configuration.json"
    configuration.write_text(
        json.dumps(
            {
                "configuration_schema_version": 1,
                "client": {
                    "transport": {"mode": "tcp", "host": "host.docker.internal", "port": port},
                    "workspace": {"path": "/data/workspace.sunny.json", "recovery": "none"},
                },
                "native": {"bridge": {"bind_host": "127.0.0.1", "port": port}},
            }
        )
    )
    configuration.chmod(0o644)
    data = args.directory / "owned workspace"
    data.mkdir()
    data.chmod(0o777)
    monkeypatch = pytest.MonkeyPatch()
    for key in (
        "SUNNY_ABLETON_HOST",
        "SUNNY_TCP_PORT",
        "SUNNY_BIND_HOST",
        "SUNNY_WORKSPACE_PATH",
        "SUNNY_WORKSPACE_RECOVERY",
    ):
        monkeypatch.delenv(key, raising=False)
    monkeypatch.setenv("SUNNY_CONFIG_PATH", str(configuration))
    live = LiveSet((12, 4, 0), midi_tracks=1).install(monkeypatch)
    callbacks = _LiveMainThread()
    surface = callbacks.call(lambda: SunnyControlSurface(object()))
    monkeypatch.setattr(surface, "song", live.surface.song, raising=False)
    surface.schedule_message = callbacks.schedule_message
    client = None
    try:
        end = time.monotonic() + 5
        while not surface._server.is_running:
            assert time.monotonic() < end, "Exported bridge startup timed out"
            time.sleep(0.01)
        assert surface._server.bound_port == port
        command = [
            "docker",
            "run",
            "-i",
            "--rm",
            "--pull=never",
            "--name",
            args.container,
            "--label",
            "com.sunny.fixture.owner=" + args.owner,
            "--mount",
            f"type=bind,source={data},target=/data",
            "--mount",
            f"type=bind,source={configuration},target=/run/sunny/configuration.json,readonly",
            "--env",
            "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
            args.image,
        ]
        client = _McpClient(None, 0, host=None, command=command)
        end = time.monotonic() + 10
        while True:
            report = client.call("doctor_ableton", request_id="released-wire-literal")
            if report.get("success") is True:
                break
            assert time.monotonic() < end, report
            time.sleep(0.1)
        assert report["request_id"] == "released-wire-literal"
        assert report["read_only_ready"] is True
        assert "Musical mutations and saved Set persistence" in report["unverified"]
        assert len(live.song.tracks) == 1 and not live.song.tracks[0].clip_slots[0].has_clip
        assert report["observed_bridge"]["source_sha256"] == BRIDGE_SOURCE_SHA256
        saved = client.call("workspace_save", path="/data/workspace.sunny.json")
        assert saved["durability_confirmed"] is True and saved["native_history_available"] is True
        created = client.call(
            "create_progression_clip",
            track_index=0,
            slot_index=0,
            root="C",
            scale="major",
            numerals=["I"],
            octave=4,
            duration_beats=4.0,
        )
        assert created["success"] is True, created
        # The expected notes are independently hand-derived C major, not taken
        # from the response or a second run of the implementation.
        notes = live.song.tracks[0].clip_slots[0].clip.get_all_notes_extended()
        assert sorted((note.pitch, note.start_time, note.duration) for note in notes) == [
            (60, 0.0, 3.6),
            (64, 0.0, 3.6),
            (67, 0.0, 3.6),
        ]
        before = tuple((note.pitch, note.start_time, note.duration) for note in notes)
        refused = client.call(
            "create_progression_clip",
            track_index=0,
            slot_index=0,
            root="D",
            scale="major",
            numerals=["I"],
            octave=4,
            duration_beats=4.0,
        )
        assert refused["success"] is False and refused["outcome"] == "not_applied", refused
        after = live.song.tracks[0].clip_slots[0].clip.get_all_notes_extended()
        assert tuple((note.pitch, note.start_time, note.duration) for note in after) == before
        log = client.call("get_ableton_remote_log")
        assert log["success"] is True and log["entries"] and log["truncated"] is False
        empty = client.call(
            "get_ableton_remote_log",
            after_sequence=log["next_sequence"],
            stream_id=log["stream_id"],
        )
        assert empty["success"] is True and empty["entries"] == [] and empty["reset"] is False

        # Lose one actual completed setter response at the socket boundary.
        # Every production layer remains in use, and independent disk evidence
        # must already contain the exact original fence before native dispatch.
        dispatch = surface._managed_registry.dispatch_legacy
        native_requests = []
        execute_tokens = []
        drop = {"token": None, "done": False}
        disk_observations = []

        def retained_ledger():
            # The actual ledger remains private mode0600 for image UID1001.
            # Observe its disk bytes through a separate readonly owned mount;
            # never relax permissions for the host test driver's UID.
            code = (
                "import json,pathlib; "
                "m=json.loads(pathlib.Path('/data/workspace.sunny.json').read_text())"
                "['native_realization']; "
                "p=pathlib.Path(m['history_base_directory'])/m['workspace_namespace']"
                "/'ledger.json'; "
                "print(p.read_text())"
            )
            observer = args.container + "-observe-" + str(len(disk_observations) + 1)
            disk_observations.append(observer)
            observed = subprocess.run(
                [
                    "docker",
                    "run",
                    "--rm",
                    "--pull=never",
                    "--network",
                    "none",
                    "--name",
                    observer,
                    "--label",
                    "com.sunny.fixture.owner=" + args.owner,
                    "--mount",
                    f"type=bind,source={data},target=/data,readonly",
                    "--entrypoint",
                    "python3",
                    args.image,
                    "-c",
                    code,
                ],
                capture_output=True,
                text=True,
                timeout=10,
            )
            assert observed.returncode == 0, observed.stderr
            return json.loads(observed.stdout)

        def observe_dispatch(name, values):
            native_requests.append(name)
            if name in {"sunny_legacy_prepare", "sunny_legacy_execute"}:
                payload = values[0]
                retained = [
                    workflow
                    for workflow in retained_ledger()["legacy_workflows"]
                    if workflow["workflow_id"] == payload["workflow_id"]
                ]
                assert len(retained) == 1
                children = [
                    child
                    for child in retained[0]["children"]
                    if child["prepared"]["intent"]["operation_id"] == payload["operation_id"]
                ]
                assert len(children) == 1 and children[0]["dispatch_state"] == "may_have_sent"
                if name == "sunny_legacy_prepare":
                    assert children[0]["prepared"]["intent"] == payload
                    assert children[0]["evidence"] == []
                else:
                    assert all(
                        children[0]["prepared"]["intent"][key] == value
                        for key, value in payload.items()
                        if key != "fingerprint"
                    )
                    assert children[0]["evidence"][-1]["journal"]["outcome"] == "prepared"
                    assert (
                        children[0]["evidence"][-1]["journal"]["fingerprint"]
                        == payload["fingerprint"]
                    )
                    execute_tokens.append(dict(payload))
                    drop["token"] = payload["operation_id"]
            return dispatch(name, values)

        monkeypatch.setattr(surface._managed_registry, "dispatch_legacy", observe_dispatch)
        send = surface._server._send_frame

        def lose_completed_reply(sock, payload):
            response = json.loads(payload)
            journal = response.get("value")
            if (
                not drop["done"]
                and isinstance(journal, dict)
                and journal.get("operation_id") == drop["token"]
                and journal.get("outcome") == "acknowledged"
            ):
                assert journal["native_mutation_started"] is True
                assert journal["started_calls"] == journal["returned_calls"] == 1
                drop["done"] = True
                surface._server._close_socket(sock)
                return
            send(sock, payload)

        monkeypatch.setattr(surface._server, "_send_frame", lose_completed_reply)
        origin = {key: report["session"][key] for key in ("bridge_instance", "document_token")}
        setter = {"type": "set", "path": "song", "name": "tempo", "args": [150.0]}
        lost = client.call("legacy_ableton_request", command=setter, **origin)
        assert lost["success"] is False, lost
        assert lost["receipt"]["outcome"] == "indeterminate", lost
        # Literal receipt v1 integers: Execute=2; SentWithoutValidResponse=1.
        assert type(lost["receipt"]["stage"]) is int and lost["receipt"]["stage"] == 2, lost
        assert type(lost["receipt"]["delivery"]) is int and lost["receipt"]["delivery"] == 1, lost
        assert drop["done"] and live.song.tempo == 150.0
        assert len(execute_tokens) == 1
        blocked = client.call("legacy_ableton_request", command=setter, **origin)
        assert blocked["success"] is False and blocked["receipt"] is None, blocked
        assert len(execute_tokens) == 1 and live.song.tempo == 150.0
        original = lost["receipt"]["intent"]
        namespace = lost["workspace_namespace"]
        client.close()
        client = None
        restarted_command = list(command)
        restarted_command[restarted_command.index(args.container)] = args.container + "-restart"
        client = _McpClient(None, 0, host=None, command=restarted_command)
        ready = client.call("doctor_ableton")
        assert ready["success"] is True and ready["session"] == report["session"], ready
        history = client.call("legacy_ableton_history", workspace_namespace=namespace)
        assert history["success"] is True and history["unresolved"] is True, history
        recovered = client.call(
            "legacy_ableton_reconcile",
            workspace_namespace=namespace,
            workflow_id=original["workflow_id"],
            operation_id=original["operation_id"],
        )
        assert recovered["success"] is True and recovered["history_saved"] is True, recovered
        assert recovered["query_only"] is True and recovered["dispatch_permit"] is False
        assert recovered["receipt"]["outcome"] == "acknowledged"
        assert recovered["receipt"]["intent"] == original
        assert recovered["receipt"]["journal"]["started_calls"] == 1
        assert recovered["receipt"]["journal"]["returned_calls"] == 1
        assert len(execute_tokens) == 1 and live.song.tempo == 150.0
        blocked_after_query = client.call("legacy_ableton_request", command=setter, **origin)
        assert blocked_after_query["success"] is False and blocked_after_query["receipt"] is None
        assert len(execute_tokens) == 1
        assert native_requests.count("sunny_legacy_prepare") == 1
        assert native_requests.count("sunny_legacy_execute") == 1
        assert native_requests.count("sunny_legacy_operation") == 1
        assert len(disk_observations) == 2
        assert tuple((note.pitch, note.start_time, note.duration) for note in after) == before
        print(
            json.dumps(
                {
                    "exported_bridge_source_sha256": BRIDGE_SOURCE_SHA256,
                    "read_only_doctor": True,
                    "literal_notes": [[60, 0.0, 3.6], [64, 0.0, 3.6], [67, 0.0, 3.6]],
                    "occupied_slot_preserved": True,
                    "incremental_log": True,
                    "configuration_schema_version": 1,
                    "lost_reply_recovered_by_original_token": True,
                    "musical_setter_entries": 1,
                    "durable_fence_before_native_dispatch": True,
                    "native_host_executed": False,
                }
            )
        )
    finally:
        if client is not None:
            client.close()
        surface.disconnect()
        callbacks.stop()
        monkeypatch.undo()


if __name__ == "__main__":
    main()
