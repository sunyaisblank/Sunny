"""End-to-end tests for indexed residual Max host evidence and native evaluation."""

from __future__ import annotations

import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
VALIDATION = REPOSITORY / "max-package/misc/validation"
PLAN = VALIDATION / "max-host-run-plan.json"
OBSERVATION = VALIDATION / "max-validation-observation.example.json"
INDEXER = VALIDATION / "index-max-host-run.py"
M4L_EXPORTER = VALIDATION / "export-m4l-assertions.py"


def _tool() -> Path:
    return Path(
        os.environ.get("SUNNY_MAX_EVIDENCE_TOOL", str(REPOSITORY / ".bin/sunny-max-evidence"))
    )


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def _write_float_wave(path: Path, channels: list[list[float]], sample_rate: int, bits: int) -> None:
    assert bits in {32, 64}
    assert channels and len({len(channel) for channel in channels}) == 1
    path.parent.mkdir(parents=True, exist_ok=True)
    sample_code = "f" if bits == 32 else "d"
    payload = bytearray()
    for frame in zip(*channels, strict=True):
        payload.extend(struct.pack(f"<{len(frame)}{sample_code}", *frame))
    channel_count = len(channels)
    bytes_per_sample = bits // 8
    block_align = channel_count * bytes_per_sample
    byte_rate = sample_rate * block_align
    wave_format = struct.pack(
        "<HHIIHH", 3, channel_count, sample_rate, byte_rate, block_align, bits
    )
    riff_size = 4 + 8 + len(wave_format) + 8 + len(payload)
    path.write_bytes(
        b"RIFF"
        + struct.pack("<I", riff_size)
        + b"WAVEfmt "
        + struct.pack("<I", len(wave_format))
        + wave_format
        + b"data"
        + struct.pack("<I", len(payload))
        + payload
    )


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _passing_evidence(root: Path) -> Path:
    observation = json.loads(OBSERVATION.read_text(encoding="utf-8"))
    observation["environment"]["overdrive"] = True
    observation["environment"]["scheduler_in_audio_interrupt"] = True
    observation_path = root / "observation.json"
    _write_json(observation_path, observation)

    evidence = root / "evidence"
    objects = []
    for name in ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"):
        objects.append(
            {
                "object_name": name,
                "attempted_cycles": 1000,
                "constructed_cycles": 1000,
                "dsp_active_cycles": 1000,
                "destroyed_cycles": 1000,
            }
        )
    _write_json(
        evidence / "active-dsp-teardown/cycle-counts.json",
        {
            "schema_version": 1,
            "objects": objects,
            "host_exit_code": 0,
            "crash_count": 0,
            "hang_count": 0,
            "invalid_access_count": 0,
            "definitely_lost_bytes": 0,
        },
    )
    (evidence / "active-dsp-teardown/host-console.txt").write_text(
        "named Max host exited normally\n", encoding="utf-8"
    )
    (evidence / "active-dsp-teardown/memory-diagnostic.txt").write_text(
        "named diagnostic: zero invalid accesses and zero definitely lost bytes\n",
        encoding="utf-8",
    )

    target_ticks = [1000 + 48 * index for index in range(32)]
    scheduler_scenarios = [
        {
            "name": "supported_enabled_timely",
            "overdrive": True,
            "scheduler_in_audio_interrupt": True,
            "downstream": "click~",
            "target_ticks": target_ticks,
            "callback_count": 32,
            "target_rejected": False,
            "message_times_ms": [],
        },
        {
            "name": "supported_without_siai",
            "overdrive": True,
            "scheduler_in_audio_interrupt": False,
            "downstream": "click~",
            "target_ticks": target_ticks,
            "callback_count": 32,
            "target_rejected": False,
            "message_times_ms": [],
        },
        {
            "name": "supported_without_overdrive",
            "overdrive": False,
            "scheduler_in_audio_interrupt": False,
            "downstream": "click~",
            "target_ticks": target_ticks,
            "callback_count": 32,
            "target_rejected": False,
            "message_times_ms": [],
        },
        {
            "name": "late_absolute_target",
            "overdrive": True,
            "scheduler_in_audio_interrupt": True,
            "downstream": "click~",
            "target_ticks": [999],
            "callback_count": 0,
            "target_rejected": True,
            "message_times_ms": [],
        },
        {
            "name": "ordinary_message_consumer",
            "overdrive": True,
            "scheduler_in_audio_interrupt": True,
            "downstream": "timer",
            "target_ticks": target_ticks,
            "callback_count": 32,
            "target_rejected": False,
            "message_times_ms": [float(index * 50) for index in range(32)],
        },
    ]
    _write_json(
        evidence / "scheduler-timing/scenario-settings.json",
        {
            "schema_version": 1,
            "tempo_bpm": 120.0,
            "ticks_per_quarter": 480,
            "event_count": 32,
            "tick_interval": 48,
            "scenarios": scheduler_scenarios,
        },
    )
    impulse_offsets = [
        [100 + 2400 * index for index in range(32)],
        [110 + 2401 * index for index in range(32)],
        [120 + 2399 * index for index in range(32)],
        [],
    ]
    timing_frames = max(offsets[-1] for offsets in impulse_offsets if offsets) + 2
    timing_channels = [[0.0] * timing_frames for _ in range(4)]
    for channel, offsets in enumerate(impulse_offsets):
        for offset in offsets:
            timing_channels[channel][offset] = 1.0
    timing_wave = evidence / "scheduler-timing/timing-capture.wav"
    _write_float_wave(timing_wave, timing_channels, 48_000, 32)
    _write_json(
        evidence / "scheduler-timing/impulse-offsets.json",
        {
            "schema_version": 1,
            "audio_sha256": _sha256(timing_wave),
            "channels": [
                {
                    "scenario": name,
                    "channel_index": index,
                    "sample_offsets": impulse_offsets[index],
                }
                for index, name in enumerate(
                    (
                        "supported_enabled_timely",
                        "supported_without_siai",
                        "supported_without_overdrive",
                        "late_absolute_target",
                    )
                )
            ],
        },
    )

    _write_json(
        evidence / "downstream-midi/ports.json",
        {
            "schema_version": 1,
            "output_port": "Sunny Test Out",
            "input_port": "Sunny Test In",
            "receiver_name": "Named MIDI Monitor",
            "receiver_version": "1.0",
            "channel": 1,
        },
    )
    _write_json(
        evidence / "downstream-midi/received-midi.json",
        {
            "schema_version": 1,
            "source_callback_lists": [[60, 100, 23], [60, 0, 23], [64, 101, 73], [64, 0, 73]],
            "received_bytes": [144, 60, 100, 128, 60, 23, 144, 64, 101, 128, 64, 73],
        },
    )
    (evidence / "downstream-midi/receiver-transcript.txt").write_text(
        "named receiver captured twelve bytes\n", encoding="utf-8"
    )

    render_frames = 4096
    render_wave = evidence / "rendered-audio/sunny-render.wav"
    _write_float_wave(
        render_wave,
        [
            [-1.0] * render_frames,
            [0.375] * render_frames,
            [-0.25] * render_frames,
            [index / 24_000.0 for index in range(render_frames)],
        ],
        48_000,
        64,
    )
    configurations = (
        "frequency 0, saw, reset",
        "attack 0, decay 0, sustain 0.375, release 0, reset, gate 1",
        "value -0.25",
        "tempo 120, position 0, play",
    )
    _write_json(
        evidence / "rendered-audio/render-settings.json",
        {
            "schema_version": 1,
            "format": "wave_ieee_float",
            "sample_rate": 48_000.0,
            "minimum_frames": 4096,
            "settled_start_frame": 0,
            "settled_frame_count": render_frames,
            "channels": [
                {
                    "object_name": name,
                    "channel_index": index,
                    "configuration": configurations[index],
                }
                for index, name in enumerate(
                    ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~")
                )
            ],
        },
    )
    _write_json(
        evidence / "rendered-audio/comparison.json",
        {
            "schema_version": 1,
            "comparator_name": "named-host-reference-comparator",
            "comparator_version": "1.0",
            "command": ["named-host-reference-comparator", "sunny-render.wav"],
            "exit_code": 0,
            "audio_sha256": _sha256(render_wave),
        },
    )
    return observation_path


def _index(evidence: Path, observation: Path) -> Path:
    indexed = subprocess.run(
        [sys.executable, str(INDEXER), str(PLAN), str(observation), str(evidence)],
        check=True,
        capture_output=True,
        text=True,
    )
    result = evidence / "host-run-result.json"
    result.write_text(indexed.stdout, encoding="utf-8")
    return result


def test_native_residual_application_reads_raw_measurements_and_wave_files(tmp_path: Path) -> None:
    """All four outcomes come from native evaluation of the content-indexed raw artifacts."""
    observation = _passing_evidence(tmp_path)
    evidence = tmp_path / "evidence"
    result = _index(evidence, observation)
    applied = subprocess.run(
        [
            str(_tool()),
            "apply-host-run",
            str(PLAN),
            str(result),
            str(observation),
            str(evidence),
            "host-run-result.json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    output = json.loads(applied.stdout)
    residuals = {
        "active_dsp_teardown",
        "scheduler_timing",
        "downstream_midi_delivery",
        "rendered_audio",
    }
    for check in output["checks"]:
        if check["check"] in residuals:
            assert check["outcome"] == "passed"
            assert check["evidence_relative_path"] == "host-run-result.json"
            assert "residual index sha256=" in check["summary"]
        elif check["check"] == "live_transport_discontinuities":
            assert check["outcome"] == "not_applicable"
        else:
            assert check["outcome"] == "not_run"

    verified = subprocess.run(
        [
            str(_tool()),
            "verify-host-run",
            str(PLAN),
            str(result),
            str(evidence),
            "host-run-result.json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    assert verified.stdout == "verified\n"

    invalid_index = json.loads(result.read_text(encoding="utf-8"))
    invalid_index["source_revision"] = "ABCDEF0"
    _write_json(result, invalid_index)
    invalid_provenance = subprocess.run(
        [
            str(_tool()),
            "verify-host-run",
            str(PLAN),
            str(result),
            str(evidence),
            "host-run-result.json",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert invalid_provenance.returncode == 5
    assert "invalid_result: indexed_result" in invalid_provenance.stderr
    result = _index(evidence, observation)

    with (evidence / "rendered-audio/sunny-render.wav").open("ab") as stream:
        stream.write(b"drift")
    rejected = subprocess.run(
        [
            str(_tool()),
            "verify-host-run",
            str(PLAN),
            str(result),
            str(evidence),
            "host-run-result.json",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert rejected.returncode == 5
    assert "evidence_digest_mismatch: rendered-audio/sunny-render.wav" in rejected.stderr


def test_complete_but_failing_residual_measurement_is_retained_as_failed(tmp_path: Path) -> None:
    """A measured byte mismatch is a failed check, not a malformed-evidence rejection."""
    observation = _passing_evidence(tmp_path)
    evidence = tmp_path / "evidence"
    received_path = evidence / "downstream-midi/received-midi.json"
    received = json.loads(received_path.read_text(encoding="utf-8"))
    received["received_bytes"][-1] = 72
    _write_json(received_path, received)
    result = _index(evidence, observation)
    applied = subprocess.run(
        [
            str(_tool()),
            "apply-host-run",
            str(PLAN),
            str(result),
            str(observation),
            str(evidence),
            "host-run-result.json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    outcomes = {check["check"]: check["outcome"] for check in json.loads(applied.stdout)["checks"]}
    assert outcomes["active_dsp_teardown"] == "passed"
    assert outcomes["scheduler_timing"] == "passed"
    assert outcomes["downstream_midi_delivery"] == "failed"
    assert outcomes["rendered_audio"] == "passed"


def test_m4l_result_requires_and_derives_six_device_context_scenarios(tmp_path: Path) -> None:
    """The conditional index derives shared assertion and transport facts from bound M4L input."""
    observation_path = _passing_evidence(tmp_path)
    observation = json.loads(observation_path.read_text(encoding="utf-8"))
    observation["harness"] = "synthetic-parser-fixture-not-host-evidence"
    observation["environment"]["host_kind"] = "max_for_live"
    observation["environment"]["live_version"] = "12.3.5"
    observation["environment"]["max_for_live_version"] = "9.0.5"
    for check in observation["checks"]:
        if check["check"] == "live_transport_discontinuities":
            check["outcome"] = "not_run"
            check["summary"] = "Synthetic parser fixture; no host claim."
        elif check["check"] == "standalone_transport_identity":
            check["outcome"] = "not_applicable"
            check["summary"] = "M4L fixture does not use standalone transport identity."
    _write_json(observation_path, observation)

    evidence = tmp_path / "evidence"
    live_directory = evidence / "live-transport-discontinuities"
    live_directory.mkdir(parents=True, exist_ok=True)
    device = live_directory / "sunny-validation-device.amxd"
    device.write_bytes(b"synthetic host-saved-device parser fixture\n")
    harness = VALIDATION / "max-test-harness.json"
    (live_directory / "max-test-harness.json").write_bytes(harness.read_bytes())
    manifest = json.loads(harness.read_text(encoding="utf-8"))
    shared_checks = {
        "package_discovery",
        "public_class_surface",
        "signal_topology",
        "dsp_setup",
        "perform_callback",
        "finite_signal_output",
        "disconnected_processing",
        "reconnect_continuity",
        "control_dispatch",
        "itm_schedule_fire",
        "itm_equal_tick_ordering",
        "itm_clear_reassign",
        "release_velocity_formatting",
    }
    assertion_names: set[str] = set()
    for mapping in manifest["artifact_assertions"]:
        assertion_names.update(mapping["required_assertions"])
    for mapping in manifest["check_assertions"]:
        if mapping["check"] in shared_checks:
            assertion_names.update(mapping["required_assertions"])
    assert len(assertion_names) == 64
    run_id = "synthetic-m4l-parser-run-1"
    console = live_directory / "host-console.txt"
    console.write_text(
        "synthetic parser fixture; not Max for Live output\n"
        f"SUNNY_M4L_ASSERTIONS_BEGIN|{run_id}\n"
        + "".join(f"SUNNY_M4L_ASSERTION|{run_id}|{name}|Pass\n" for name in sorted(assertion_names))
        + f"SUNNY_M4L_ASSERTIONS_END|{run_id}|64\n",
        encoding="utf-8",
    )
    exported = subprocess.run(
        [sys.executable, str(M4L_EXPORTER), str(harness), str(device), str(console), run_id],
        check=True,
        capture_output=True,
        text=True,
    )
    (live_directory / "shared-assertions.json").write_text(exported.stdout, encoding="utf-8")
    scenario_names = (
        "stop_start",
        "seek_forward_and_backward",
        "arrangement_loop",
        "tempo_change",
        "preview_enter_exit",
        "scheduler_settings",
    )
    _write_json(
        live_directory / "scenario-results.json",
        {
            "schema_version": 1,
            "live_set_name": "Synthetic Parser Fixture",
            "device_sha256": _sha256(device),
            "assertion_run_id": run_id,
            "scenarios": [
                {
                    "name": name,
                    "completed": True,
                    "scheduled_event_count": 2,
                    "expected_fire_count": 1,
                    "observed_fire_count": 1,
                    "duplicate_fire_count": 0,
                    "stale_output_count": 0,
                    "callback_healthy": True,
                    "final_reserved_slots": 0,
                    "final_retained_events": 0,
                    "observation": f"Synthetic structured fixture for {name}; not host evidence.",
                }
                for name in scenario_names
            ],
        },
    )
    result = _index(evidence, observation_path)
    applied = subprocess.run(
        [
            str(_tool()),
            "apply-host-run",
            str(PLAN),
            str(result),
            str(observation_path),
            str(evidence),
            "host-run-result.json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    output = json.loads(applied.stdout)
    outcomes = {check["check"]: check["outcome"] for check in output["checks"]}
    assert outcomes["live_transport_discontinuities"] == "passed"
    assert outcomes["standalone_transport_identity"] == "not_applicable"
    assert all(outcomes[check] == "passed" for check in shared_checks)
    assert all(
        artifact["discovered"] is True and artifact["instantiated"] is True
        for artifact in output["artifacts"]
    )

    shared_path = live_directory / "shared-assertions.json"
    shared = json.loads(shared_path.read_text(encoding="utf-8"))
    sample_rate = next(
        assertion
        for assertion in shared["assertions"]
        if assertion["name"] == "lfo:sample-rate-positive"
    )
    sample_rate["outcome"] = "Fail"
    _write_json(shared_path, shared)
    failed_result = _index(evidence, observation_path)
    failed = subprocess.run(
        [
            str(_tool()),
            "apply-host-run",
            str(PLAN),
            str(failed_result),
            str(observation_path),
            str(evidence),
            "host-run-result.json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    failed_outcomes = {
        check["check"]: check["outcome"] for check in json.loads(failed.stdout)["checks"]
    }
    assert failed_outcomes["dsp_setup"] == "failed"
    assert failed_outcomes["package_discovery"] == "passed"
