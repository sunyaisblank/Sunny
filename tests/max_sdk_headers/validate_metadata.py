"""Validate authored Max package metadata and wrapper/reference agreement."""

from __future__ import annotations

import hashlib
import json
import re
import xml.etree.ElementTree as element_tree
from pathlib import Path


def _patcher_objects(document: dict[str, object]) -> dict[str, dict[str, object]]:
    """Return one Max patcher's boxes keyed by its closed authored identifier."""
    patcher = document.get("patcher")
    if not isinstance(patcher, dict) or not isinstance(patcher.get("boxes"), list):
        raise ValueError("invalid Max patcher root")
    result: dict[str, dict[str, object]] = {}
    for encoded in patcher["boxes"]:
        if not isinstance(encoded, dict) or not isinstance(encoded.get("box"), dict):
            raise ValueError("invalid Max patcher box")
        box = encoded["box"]
        identifier = box.get("id")
        if not isinstance(identifier, str) or identifier in result:
            raise ValueError("missing or duplicate Max patcher box id")
        result[identifier] = box
    return result


def _patcher_edges(document: dict[str, object]) -> set[tuple[str, int, str, int]]:
    """Return exact source/destination tuples from one authored Max patcher."""
    patcher = document.get("patcher")
    if not isinstance(patcher, dict) or not isinstance(patcher.get("lines"), list):
        raise ValueError("invalid Max patcher line set")
    result: set[tuple[str, int, str, int]] = set()
    for encoded in patcher["lines"]:
        if not isinstance(encoded, dict) or not isinstance(encoded.get("patchline"), dict):
            raise ValueError("invalid Max patchline")
        patchline = encoded["patchline"]
        source = patchline.get("source")
        destination = patchline.get("destination")
        if (
            not isinstance(source, list)
            or not isinstance(destination, list)
            or len(source) != 2
            or len(destination) != 2
            or not isinstance(source[0], str)
            or not isinstance(source[1], int)
            or not isinstance(destination[0], str)
            or not isinstance(destination[1], int)
        ):
            raise ValueError("invalid Max patchline endpoints")
        result.add((source[0], source[1], destination[0], destination[1]))
    return result


def _message_selectors(text: str) -> set[str]:
    """Extract selectors from one comma-ordered Max message box."""
    result: set[str] = set()
    for message in text.split(","):
        selector = message.strip().split()[0]
        try:
            float(selector)
        except ValueError:
            result.add(selector)
        else:
            result.add("float")
    return result


def main() -> None:
    """Reject malformed metadata or divergence from registered wrapper methods."""
    repository = Path(__file__).resolve().parents[2]
    package = repository / "max-package"
    manifest = json.loads((package / "package-info.json").read_text(encoding="utf-8"))
    if manifest["name"] != "Sunny" or manifest["version"] != "0.4.0":
        raise ValueError("Max manifest identity/version diverges from Sunny 0.4.0")
    if "transport" in manifest.get("tags", []):
        raise ValueError("local sunny.clock~ must not be advertised as a host transport")

    validation_example = json.loads(
        (package / "misc" / "validation" / "max-validation-record.example.json").read_text(
            encoding="utf-8"
        )
    )
    required_host_checks = (
        "package_discovery",
        "public_class_surface",
        "signal_topology",
        "dsp_setup",
        "perform_callback",
        "finite_signal_output",
        "disconnected_processing",
        "reconnect_continuity",
        "control_dispatch",
        "active_dsp_teardown",
        "itm_schedule_fire",
        "itm_equal_tick_ordering",
        "itm_clear_reassign",
        "release_velocity_formatting",
        "scheduler_timing",
        "downstream_midi_delivery",
        "rendered_audio",
        "live_transport_discontinuities",
        "standalone_transport_identity",
    )
    if (
        validation_example.get("schema_version") != 1
        or validation_example.get("sunny_version") != manifest["version"]
        or validation_example.get("package_version") != manifest["version"]
        or validation_example.get("harness") != "example_not_evidence"
        or validation_example.get("complete") is not False
        or tuple(check.get("check") for check in validation_example.get("checks", ()))
        != required_host_checks
    ):
        raise ValueError(
            "Max package validation handoff is stale, incomplete, or overclaims evidence"
        )

    observation_example = json.loads(
        (package / "misc" / "validation" / "max-validation-observation.example.json").read_text(
            encoding="utf-8"
        )
    )
    if (
        observation_example.get("schema_version") != 1
        or observation_example.get("sunny_version") != manifest["version"]
        or observation_example.get("package_version") != manifest["version"]
        or observation_example.get("harness") != "example_observation_not_evidence"
        or "package_archive_sha256" in observation_example
        or "complete" in observation_example
        or tuple(check.get("check") for check in observation_example.get("checks", ()))
        != required_host_checks
        or any("binary_sha256" in artifact for artifact in observation_example.get("artifacts", ()))
        or any("evidence_sha256" in check for check in observation_example.get("checks", ()))
    ):
        raise ValueError("Max package observation handoff is stale or contains derived claims")

    release_matrix_path = package / "misc" / "validation" / "max-release-matrix.json"
    release_matrix_bytes = release_matrix_path.read_bytes()
    release_matrix = json.loads(release_matrix_bytes)
    release_matrix_digest = hashlib.sha256(release_matrix_bytes).hexdigest()
    native_release_matrix_contract = (
        repository / "include/sunny/infrastructure/max/release_matrix.hpp"
    ).read_text(encoding="utf-8")
    expected_release_cells = (
        ("macos", "x86_64", "standalone_max", "records/macos-x86_64-standalone-max.json"),
        ("macos", "x86_64", "max_for_live", "records/macos-x86_64-max-for-live.json"),
        ("macos", "arm64", "standalone_max", "records/macos-arm64-standalone-max.json"),
        ("macos", "arm64", "max_for_live", "records/macos-arm64-max-for-live.json"),
        ("windows", "x86_64", "standalone_max", "records/windows-x86_64-standalone-max.json"),
        ("windows", "x86_64", "max_for_live", "records/windows-x86_64-max-for-live.json"),
    )
    observed_release_cells = tuple(
        (
            cell.get("operating_system"),
            cell.get("architecture"),
            cell.get("host_kind"),
            cell.get("record_relative_path"),
        )
        for cell in release_matrix.get("cells", ())
    )
    if (
        set(release_matrix)
        != {
            "schema_version",
            "matrix_name",
            "sunny_version",
            "coverage",
            "cells",
        }
        or release_matrix.get("schema_version") != 1
        or release_matrix.get("matrix_name") != "sunny-max-supported-hosts"
        or release_matrix.get("sunny_version") != manifest["version"]
        or release_matrix.get("coverage")
        != {
            "unit": "one_complete_record_per_supported_platform_architecture_and_host_kind",
            "configuration_scope": "recorded_environment_only",
            "underlying_evidence": "verified_separately_per_record",
        }
        or observed_release_cells != expected_release_cells
        or f'"{release_matrix_digest}"' not in native_release_matrix_contract
    ):
        raise ValueError("Max release matrix topology or compiled digest authority is stale")

    if manifest["os"]["macintosh"]["platform"] != ["x64", "aarch64"] or manifest["os"]["windows"][
        "platform"
    ] != ["x64"]:
        raise ValueError("Max release matrix targets diverge from package-info platform support")

    host_plan_path = package / "misc" / "validation" / "max-host-run-plan.json"
    host_plan_bytes = host_plan_path.read_bytes()
    host_plan = json.loads(host_plan_bytes)
    host_plan_digest = hashlib.sha256(host_plan_bytes).hexdigest()
    native_host_plan_contract = (
        repository / "include/sunny/infrastructure/max/host_run_result.hpp"
    ).read_text(encoding="utf-8")
    host_plan_keys = {
        "schema_version",
        "plan_name",
        "sunny_version",
        "release_manifest",
        "observation_template",
        "baseline_environment",
        "official_contracts",
        "max_test",
        "max_for_live_assertions",
        "evidence_directories",
        "retained_artifacts",
        "normalized_residual_result",
        "residual_checks",
        "max_for_live_transport_scenarios",
        "named_live_run",
    }
    pinned_host_runner = host_plan.get("max_test", {})
    live_assertion_contract = host_plan.get("max_for_live_assertions", {})
    normalized_residual = host_plan.get("normalized_residual_result", {})
    residual_checks = host_plan.get("residual_checks", ())
    transport_scenarios = host_plan.get("max_for_live_transport_scenarios", ())
    if (
        set(host_plan) != host_plan_keys
        or host_plan.get("schema_version") != 1
        or host_plan.get("plan_name") != "sunny-max-named-host-run"
        or host_plan.get("sunny_version") != manifest["version"]
        or host_plan.get("release_manifest") != "max-release-matrix.json"
        or host_plan.get("observation_template") != "max-validation-observation.example.json"
        or f'"{host_plan_digest}"' not in native_host_plan_contract
        or host_plan.get("baseline_environment")
        != {
            "sample_rate": 48000.0,
            "io_vector_size": 512,
            "signal_vector_size": 64,
            "overdrive": True,
            "scheduler_in_audio_interrupt": True,
        }
        or pinned_host_runner.get("revision") != "8c5d833d4b1e454238ced7c866c34acedb69cc07"
        or pinned_host_runner.get("max_sdk_base_revision")
        != "b6d635cc69bac680c35a63fcfebba9d523ab0d6c"
        or pinned_host_runner.get("package_version") != "1.2.1"
        or pinned_host_runner.get("configuration") != {"port-send": 4792, "port-listen": 4791}
        or pinned_host_runner.get("macos_architecture_arguments") != ["x86_64", "arm64"]
        or live_assertion_contract.get("device_kind") != "max_audio_effect"
        or live_assertion_contract.get("source_harness") != "max-test-harness.json"
        or live_assertion_contract.get("source_harness_sha256")
        != "1d26434e9c3a00b5604070c9bd950755365c2a82a7f642fd205fb17d95db39bc"
        or live_assertion_contract.get("source_generator") != "prepare-m4l-device-source.py"
        or live_assertion_contract.get("console_exporter") != "export-m4l-assertions.py"
        or live_assertion_contract.get("pinned_console_source_sha256")
        != "fe8084df5a1350380f18d9299714865d6a68d2e28bb3cfbe5fbf1398f2df3028"
        or live_assertion_contract.get("protocol")
        != {
            "begin": "SUNNY_M4L_ASSERTIONS_BEGIN|<run-id>",
            "assertion": "SUNNY_M4L_ASSERTION|<run-id>|<name>|Pass-or-Fail",
            "end": "SUNNY_M4L_ASSERTIONS_END|<run-id>|64",
        }
        or live_assertion_contract.get("assertion_count") != 64
        or tuple(live_assertion_contract.get("shared_checks", ()))
        != (
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
        )
        or host_plan.get("evidence_directories")
        != [
            "max-test",
            "active-dsp-teardown",
            "scheduler-timing",
            "downstream-midi",
            "rendered-audio",
            "live-transport-discontinuities",
        ]
        or normalized_residual.get("schema_version") != 1
        or normalized_residual.get("indexer") != "index-max-host-run.py"
        or normalized_residual.get("evidence_relative_path") != "host-run-result.json"
        or tuple(
            (entry.get("role"), entry.get("relative_path"))
            for entry in normalized_residual.get("file_roles", ())
        )
        != (
            ("teardown_counts", "active-dsp-teardown/cycle-counts.json"),
            ("teardown_console", "active-dsp-teardown/host-console.txt"),
            (
                "teardown_memory_diagnostic",
                "active-dsp-teardown/memory-diagnostic.txt",
            ),
            ("scheduler_settings", "scheduler-timing/scenario-settings.json"),
            ("scheduler_offsets", "scheduler-timing/impulse-offsets.json"),
            ("scheduler_audio", "scheduler-timing/timing-capture.wav"),
            ("midi_ports", "downstream-midi/ports.json"),
            ("midi_received", "downstream-midi/received-midi.json"),
            (
                "midi_receiver_transcript",
                "downstream-midi/receiver-transcript.txt",
            ),
            ("render_settings", "rendered-audio/render-settings.json"),
            ("render_audio", "rendered-audio/sunny-render.wav"),
            (
                "render_comparison_transcript",
                "rendered-audio/comparison.json",
            ),
        )
        or tuple(
            (entry.get("role"), entry.get("relative_path"))
            for entry in normalized_residual.get("max_for_live_file_roles", ())
        )
        != (
            (
                "live_saved_device",
                "live-transport-discontinuities/sunny-validation-device.amxd",
            ),
            (
                "live_assertion_contract",
                "live-transport-discontinuities/max-test-harness.json",
            ),
            (
                "live_shared_assertions",
                "live-transport-discontinuities/shared-assertions.json",
            ),
            (
                "live_scenario_results",
                "live-transport-discontinuities/scenario-results.json",
            ),
            ("live_console", "live-transport-discontinuities/host-console.txt"),
        )
        or tuple(check.get("check") for check in residual_checks)
        != (
            "active_dsp_teardown",
            "scheduler_timing",
            "downstream_midi_delivery",
            "rendered_audio",
        )
        or residual_checks[0].get("minimum_cycles_per_object") != 1000
        or residual_checks[1].get("event_count") != 32
        or residual_checks[1].get("expected_sample_interval_at_baseline") != 2400
        or residual_checks[2].get("source_callback_lists")
        != [[60, 100, 23], [60, 0, 23], [64, 101, 73], [64, 0, 73]]
        or residual_checks[2].get("expected_received_bytes")
        != [144, 60, 100, 128, 60, 23, 144, 64, 101, 128, 64, 73]
        or residual_checks[3].get("minimum_frames") != 4096
        or tuple(scenario.get("name") for scenario in transport_scenarios)
        != (
            "stop_start",
            "seek_forward_and_backward",
            "arrangement_loop",
            "tempo_change",
            "preview_enter_exit",
            "scheduler_settings",
        )
        or host_plan.get("named_live_run")
        != {
            "runner": "run-named-live-validation.py",
            "default_mode": "plan_only",
            "apply_requires_flag": "--apply",
            "required_result": (
                "A non-null strict AbletonValidationRecord from a consumed guarded plan, "
                "retained with the complete JSON-RPC transcript, installed server and Remote "
                "Script tree digests, and before/after Live log snapshots."
            ),
        }
    ):
        raise ValueError(
            "Max named-host operator plan is open, stale, or weakens measured criteria"
        )

    harness_manifest_path = package / "misc" / "validation" / "max-test-harness.json"
    harness_manifest_bytes = harness_manifest_path.read_bytes()
    harness_manifest = json.loads(harness_manifest_bytes)
    harness_manifest_digest = hashlib.sha256(harness_manifest_bytes).hexdigest()
    native_max_test_contract = (
        repository / "include/sunny/infrastructure/max/max_test_result.hpp"
    ).read_text(encoding="utf-8")
    if f'"{harness_manifest_digest}"' not in native_max_test_contract:
        raise ValueError("native max-test manifest digest authority is stale")
    m4l_generator = (package / "misc" / "validation" / "prepare-m4l-device-source.py").read_text(
        encoding="utf-8"
    )
    m4l_exporter = (package / "misc" / "validation" / "export-m4l-assertions.py").read_text(
        encoding="utf-8"
    )
    if (
        any(
            harness_manifest_digest not in source or "SUNNY_M4L_ASSERTION" not in source
            for source in (m4l_generator, m4l_exporter)
        )
        or "SUNNY_M4L_ASSERTIONS_BEGIN" not in m4l_generator
        or "SUNNY_M4L_ASSERTIONS_END" not in m4l_generator
        or "(?P<boundary>BEGIN|END)" not in m4l_exporter
    ):
        raise ValueError("M4L source/export protocol is not bound to the pinned assertion map")
    harness_identity = harness_manifest.get("harness", {})
    expected_harness_patchers = (
        "patchers/sunny-runtime-smoke.maxtest.maxpat",
        "patchers/sunny.assert-host-status.maxpat",
        "patchers/sunny.assert-event-status.maxpat",
        "patchers/sunny.assert-event-list.maxpat",
        "patchers/sunny.assert-host-lifecycle.maxpat",
    )
    if (
        set(harness_manifest)
        != {
            "schema_version",
            "harness",
            "patchers",
            "test_runs",
            "artifact_assertions",
            "check_assertions",
        }
        or harness_manifest.get("schema_version") != 1
        or harness_identity.get("name") != "cycling74-max-test"
        or harness_identity.get("repository") != "https://github.com/Cycling74/max-test.git"
        or harness_identity.get("revision") != "8c5d833d4b1e454238ced7c866c34acedb69cc07"
        or harness_identity.get("package_version") != "1.2.1"
        or harness_identity.get("minimum_max_version") != "8.2"
        or harness_identity.get("host_kind") != "standalone_max"
        or tuple(harness_manifest.get("patchers", ())) != expected_harness_patchers
    ):
        raise ValueError("Sunny's official max-test harness identity or pin is not exact")
    result_database = harness_identity.get("result_database", {})
    if result_database != {
        "format": "sqlite3",
        "test_table": "tests",
        "test_id_column": "test_id",
        "test_name_column": "test_name",
        "test_start_column": "test_start",
        "test_finish_column": "test_finish",
        "assertion_table": "assertions",
        "assertion_id_column": "assertion_id",
        "assertion_test_id_column": "test_id_ext",
        "assertion_name_column": "assertion_name",
        "assertion_outcome_column": "assertion_value",
        "assertion_finish_column": "assertion_finish",
        "admitted_outcomes": ["Pass", "Fail"],
    }:
        raise ValueError("Sunny's max-test result database contract is open or stale")

    patcher_documents = {
        relative: json.loads((package / relative).read_text(encoding="utf-8"))
        for relative in expected_harness_patchers
    }
    for relative, document in patcher_documents.items():
        authored_objects = _patcher_objects(document)
        authored_edges = _patcher_edges(document)
        authored_ids = set(authored_objects)
        if any(
            source not in authored_ids or destination not in authored_ids
            for source, _, destination, _ in authored_edges
        ):
            raise ValueError(f"{relative} contains a patchline with an absent endpoint")
    smoke = patcher_documents[expected_harness_patchers[0]]
    smoke_objects = _patcher_objects(smoke)
    smoke_edges = _patcher_edges(smoke)
    if (
        smoke_objects.get("load", {}).get("text") != "loadbang"
        or smoke_objects.get("terminate", {}).get("text") != "test.terminate"
        or smoke_objects.get("console", {}).get("text") != "CheckConsoleClear sunny-runtime-smoke"
        or ("load", 0, "init-order", 0) not in smoke_edges
        or ("console", 0, "terminate", 0) not in smoke_edges
    ):
        raise ValueError("Sunny's .maxtest patcher is not self-starting and self-terminating")

    host_assertion_document = patcher_documents[expected_harness_patchers[1]]
    event_assertion_document = patcher_documents[expected_harness_patchers[2]]
    event_list_assertion_document = patcher_documents[expected_harness_patchers[3]]
    lifecycle_assertion_document = patcher_documents[expected_harness_patchers[4]]
    host_assertion_templates = {
        str(box.get("text"))[len("test.assert #1:") :]
        for box in _patcher_objects(host_assertion_document).values()
        if str(box.get("text", "")).startswith("test.assert #1:")
    }
    event_assertion_templates = {
        str(box.get("text"))[len("test.assert #1:") :]
        for box in _patcher_objects(event_assertion_document).values()
        if str(box.get("text", "")).startswith("test.assert #1:")
    }
    event_list_objects = _patcher_objects(event_list_assertion_document)
    host_assertion_objects = _patcher_objects(host_assertion_document)
    lifecycle_assertion_objects = _patcher_objects(lifecycle_assertion_document)
    event_list_assertion_templates = {
        str(box.get("text"))
        for box in event_list_objects.values()
        if str(box.get("text", "")).startswith("test.assert ")
    }
    lifecycle_assertion_templates = {
        str(box.get("text"))[len("test.assert #1:") :]
        for box in lifecycle_assertion_objects.values()
        if str(box.get("text", "")).startswith("test.assert #1:")
    }
    if (
        host_assertion_templates
        != {
            "configured",
            "sample-rate-positive",
            "maximum-frames-positive",
            "process-observed",
            "last-frame-positive",
            "process-healthy",
            "last-error-zero",
        }
        or event_assertion_templates
        != {
            "command-observed",
            "scheduled-observed",
            "fired-observed",
            "cancelled-observed",
            "callbacks-healthy",
            "reservations-empty",
            "retained-empty",
            "last-error-zero",
            "transport-position-nonnegative",
            "transport-resolution-positive",
            "transport-running",
        }
        or event_list_assertion_templates != {"test.assert #4:list"}
        or lifecycle_assertion_templates
        != {"disconnected-process-advanced", "reconnected-process-advanced"}
    ):
        raise ValueError("Sunny's max-test machine-status assertions are not closed")
    if (
        host_assertion_objects.get("once", {}).get("text") != "onebang"
        or host_assertion_objects.get("unpack", {}).get("text") != "unpack i f i i i i i i"
    ):
        raise ValueError("Sunny's one-shot host-status assertion helper is open or stale")
    required_lifecycle_helper_objects = {
        "route": "route baseline disconnected reconnected host_status",
        "phase-baseline": "1",
        "phase-disconnected": "2",
        "phase-reconnected": "3",
        "unpack": "unpack i f i i i i i i",
        "phase-gate": "gate 3",
        "baseline-store": "int",
        "disconnected-order": "t i i b",
        "disconnected-store": "int",
        "disconnected-compare": ">",
        "reconnected-order": "t i b",
        "reconnected-compare": ">",
        "assert-disconnected": "test.assert #1:disconnected-process-advanced",
        "assert-reconnected": "test.assert #1:reconnected-process-advanced",
    }
    required_lifecycle_helper_edges = {
        ("in", 0, "route", 0),
        ("route", 0, "phase-baseline", 0),
        ("route", 1, "phase-disconnected", 0),
        ("route", 2, "phase-reconnected", 0),
        ("route", 3, "unpack", 0),
        ("phase-baseline", 0, "phase-gate", 0),
        ("phase-disconnected", 0, "phase-gate", 0),
        ("phase-reconnected", 0, "phase-gate", 0),
        ("unpack", 4, "phase-gate", 1),
        ("phase-gate", 0, "baseline-store", 1),
        ("phase-gate", 1, "disconnected-order", 0),
        ("disconnected-order", 2, "baseline-store", 0),
        ("baseline-store", 0, "disconnected-compare", 1),
        ("disconnected-order", 1, "disconnected-store", 1),
        ("disconnected-order", 0, "disconnected-compare", 0),
        ("disconnected-compare", 0, "assert-disconnected", 0),
        ("phase-gate", 2, "reconnected-order", 0),
        ("reconnected-order", 1, "disconnected-store", 0),
        ("disconnected-store", 0, "reconnected-compare", 1),
        ("reconnected-order", 0, "reconnected-compare", 0),
        ("reconnected-compare", 0, "assert-reconnected", 0),
    }
    if any(
        lifecycle_assertion_objects.get(identifier, {}).get("text") != text
        for identifier, text in required_lifecycle_helper_objects.items()
    ) or not required_lifecycle_helper_edges.issubset(_patcher_edges(lifecycle_assertion_document)):
        raise ValueError("Sunny's exact process-count lifecycle helper is open or malformed")
    if (
        event_list_objects.get("expected", {}).get("text") != "#1 #2 #3"
        or event_list_objects.get("compare", {}).get("text") != "zl compare"
        or event_list_objects.get("load", {}).get("text") != "loadbang"
    ):
        raise ValueError("Sunny's exact event-list assertion helper is open or malformed")

    direct_assertions = {
        str(box.get("text"))[len("test.assert ") :]
        for box in smoke_objects.values()
        if str(box.get("text", "")).startswith("test.assert ")
    }
    produced_assertions = set(direct_assertions)
    produced_assertions.add("sunny-runtime-smoke:Console")
    helper_prefixes = {"lfo", "adsr", "hold", "clock"}
    produced_assertions.update(
        f"{prefix}:{assertion}"
        for prefix in helper_prefixes
        for assertion in host_assertion_templates
    )
    produced_assertions.update(
        f"{prefix}:{assertion}"
        for prefix in helper_prefixes
        for assertion in lifecycle_assertion_templates
    )
    produced_assertions.update(f"events:{assertion}" for assertion in event_assertion_templates)
    expected_event_list_instances = {
        "assert-first": "sunny.assert-event-list 60 100 23 itm-first",
        "assert-equal-off": "sunny.assert-event-list 62 0 47 itm-equal-off",
        "assert-equal-on": "sunny.assert-event-list 61 100 31 itm-equal-on",
        "assert-note-on": "sunny.assert-event-list 63 110 59 itm-note-on",
        "assert-note-off": "sunny.assert-event-list 63 0 59 itm-note-off",
        "assert-replacement": "sunny.assert-event-list 65 101 73 itm-replacement",
    }
    if any(
        smoke_objects.get(identifier, {}).get("text") != text
        for identifier, text in expected_event_list_instances.items()
    ):
        raise ValueError("Sunny's transport trace expectations are not exact")
    produced_assertions.update(
        f"{text.split()[4]}:list" for text in expected_event_list_instances.values()
    )

    required_transport_objects = {
        "transport-order": "t b b b b b b",
        "host-transport": "transport",
        "watchdog": "delay 1500",
        "event-order": "t l b",
        "event-count": "counter 1 6",
        "event-gate": "gate 6",
        "fifth-order": "t b l",
        "sixth-order": "t b l",
        "events-final-order": "t b b b b b",
        "events-final-status": "status, transport_status",
        "reassign-controls": "event_after 96 64 100 71, clear, event_after 192 65 101 73",
        "completion-assert": "test.assert events:transport-sequence-complete",
    }
    required_transport_edges = {
        ("lifecycle-success-order", 0, "transport-order", 0),
        ("transport-order", 2, "events-controls", 0),
        ("transport-order", 1, "transport-start", 0),
        ("transport-order", 0, "watchdog", 0),
        ("events", 0, "event-order", 0),
        ("event-order", 1, "event-count", 0),
        ("event-count", 0, "event-gate", 0),
        ("event-order", 0, "event-gate", 1),
        ("event-gate", 4, "fifth-order", 0),
        ("fifth-order", 0, "reassign-controls", 0),
        ("reassign-controls", 0, "events", 0),
        ("event-gate", 5, "sixth-order", 0),
        ("sixth-order", 0, "events-final-order", 0),
        ("events-final-order", 4, "events-final-status", 0),
        ("events-final-status", 0, "events", 0),
        ("events-final-order", 2, "watchdog-stop", 0),
        ("events-final-order", 1, "completion-pass", 0),
        ("events-final-order", 0, "console", 0),
        ("watchdog-order", 2, "completion-fail", 0),
        ("watchdog-order", 0, "console", 0),
    }
    if any(
        smoke_objects.get(identifier, {}).get("text") != text
        for identifier, text in required_transport_objects.items()
    ) or not required_transport_edges.issubset(smoke_edges):
        raise ValueError("Sunny's callback-driven global-transport harness is incomplete")

    expected_lifecycle_varnames = {
        "lfo": "sunny_lfo_source",
        "lfo-sample": "sunny_lfo_sink",
        "adsr": "sunny_adsr_source",
        "adsr-sample": "sunny_adsr_sink",
        "hold": "sunny_hold_source",
        "hold-sample": "sunny_hold_sink",
        "clock": "sunny_clock_source",
        "clock-sample": "sunny_clock_sink",
    }
    required_lifecycle_objects = {
        "final-order": "t b b b b b",
        "patcher-script": "thispatcher",
        "disconnect-delay": "delay 120",
        "disconnected-order": "t b b b",
        "reconnect-command-order": "t b b",
        "reconnect-delay": "delay 120",
        "reconnected-order": "t b b b",
        "reconnect-sample-order": "t b b",
        "reconnect-sample-route": "2",
        "lifecycle-watchdog": "delay 2000",
        "lifecycle-watchdog-order": "t b b b",
        "lifecycle-watchdog-stop": "stop",
        "lifecycle-completion-fail": "0",
        "lifecycle-completion-pass": "1",
        "reconnect-count": "counter 1 4",
        "reconnect-complete": "sel 4",
        "lifecycle-success-order": "t b b b",
        "lifecycle-completion-assert": "test.assert lifecycle:sequence-complete",
    }
    required_lifecycle_edges = {
        ("final-order", 4, "lifecycle-watchdog", 0),
        ("final-order", 3, "lifecycle-baseline", 0),
        ("final-order", 2, "status", 0),
        ("final-order", 1, "disconnect-script", 0),
        ("final-order", 0, "disconnect-delay", 0),
        ("disconnect-script", 0, "patcher-script", 0),
        ("disconnect-delay", 0, "disconnected-order", 0),
        ("disconnected-order", 2, "lifecycle-disconnected", 0),
        ("disconnected-order", 1, "status", 0),
        ("disconnected-order", 0, "reconnect-command-order", 0),
        ("reconnect-command-order", 1, "reconnect-script", 0),
        ("reconnect-script", 0, "patcher-script", 0),
        ("reconnect-command-order", 0, "reconnect-delay", 0),
        ("reconnect-delay", 0, "reconnected-order", 0),
        ("reconnected-order", 2, "lifecycle-reconnected", 0),
        ("reconnected-order", 1, "status", 0),
        ("reconnected-order", 0, "reconnect-sample-order", 0),
        ("reconnect-sample-order", 1, "reconnect-sample-route", 0),
        ("reconnect-sample-order", 0, "reconnect-sample-bang", 0),
        ("reconnect-count", 0, "reconnect-complete", 0),
        ("reconnect-complete", 0, "lifecycle-success-order", 0),
        ("lifecycle-success-order", 2, "lifecycle-watchdog-stop", 0),
        ("lifecycle-watchdog-stop", 0, "lifecycle-watchdog", 0),
        ("lifecycle-success-order", 1, "lifecycle-completion-pass", 0),
        ("lifecycle-completion-pass", 0, "lifecycle-completion-assert", 0),
        ("lifecycle-watchdog", 0, "lifecycle-watchdog-order", 0),
        ("lifecycle-watchdog-order", 2, "lifecycle-completion-fail", 0),
        ("lifecycle-completion-fail", 0, "lifecycle-completion-assert", 0),
        ("lifecycle-watchdog-order", 1, "transport-stop", 0),
        ("lifecycle-watchdog-order", 0, "console", 0),
    }
    lifecycle_instances = {
        "lfo": ("sunny.assert-host-lifecycle lfo", "lfo", "lfo-sample"),
        "adsr": ("sunny.assert-host-lifecycle adsr", "adsr", "adsr-sample"),
        "hold": ("sunny.assert-host-lifecycle hold", "hold", "hold-sample"),
        "clock": ("sunny.assert-host-lifecycle clock", "clock", "clock-sample"),
    }
    for prefix, (helper_text, source_id, sample_id) in lifecycle_instances.items():
        helper_id = f"{prefix}-lifecycle"
        gate_id = f"{prefix}-sample-gate"
        order_id = f"{prefix}-reconnect-order"
        equals_id = f"{prefix}-reconnect-equals"
        assertion_id = f"{prefix}-reconnect-assert"
        if (
            smoke_objects.get(helper_id, {}).get("text") != helper_text
            or smoke_objects.get(gate_id, {}).get("text") != "gate 2 1"
            or smoke_objects.get(order_id, {}).get("text") != "t b f"
            or smoke_objects.get(assertion_id, {}).get("text")
            != f"test.assert sunny-{prefix}-reconnected-output"
        ):
            raise ValueError("Sunny's lifecycle instance or reconnect assertion is stale")
        required_lifecycle_edges.update(
            {
                ("lifecycle-baseline", 0, helper_id, 0),
                ("lifecycle-disconnected", 0, helper_id, 0),
                ("lifecycle-reconnected", 0, helper_id, 0),
                (source_id, 1, helper_id, 0),
                (source_id, 0, sample_id, 0),
                (sample_id, 0, gate_id, 0),
                ("reconnect-sample-route", 0, gate_id, 0),
                ("reconnect-sample-bang", 0, sample_id, 0),
                (gate_id, 1, order_id, 0),
                (order_id, 1, equals_id, 0),
                (equals_id, 0, assertion_id, 0),
                (order_id, 0, "reconnect-count", 0),
            }
        )
    disconnect_script = str(smoke_objects.get("disconnect-script", {}).get("text", ""))
    reconnect_script = str(smoke_objects.get("reconnect-script", {}).get("text", ""))
    for source_name, sink_name in (
        ("sunny_lfo_source", "sunny_lfo_sink"),
        ("sunny_adsr_source", "sunny_adsr_sink"),
        ("sunny_hold_source", "sunny_hold_sink"),
        ("sunny_clock_source", "sunny_clock_sink"),
    ):
        if f"script disconnect {source_name} 0 {sink_name} 0" not in disconnect_script or (
            f"script connect {source_name} 0 {sink_name} 0" not in reconnect_script
        ):
            raise ValueError("Sunny's lifecycle script does not cover every signal wrapper")
    if (
        any(
            smoke_objects.get(identifier, {}).get("varname") != varname
            for identifier, varname in expected_lifecycle_varnames.items()
        )
        or any(
            smoke_objects.get(identifier, {}).get("text") != text
            for identifier, text in required_lifecycle_objects.items()
        )
        or not required_lifecycle_edges.issubset(smoke_edges)
    ):
        raise ValueError("Sunny's disconnected/reconnected lifecycle harness is incomplete")

    artifact_mappings = harness_manifest.get("artifact_assertions", ())
    if tuple(mapping.get("object_name") for mapping in artifact_mappings) != (
        "sunny.lfo~",
        "sunny.adsr~",
        "sunny.hold~",
        "sunny.clock~",
        "sunny.events",
    ) or any(
        not mapping.get("required_assertions")
        or not set(mapping["required_assertions"]).issubset(produced_assertions)
        for mapping in artifact_mappings
    ):
        raise ValueError("Max artifact discovery is not derived from closed harness assertions")

    check_mappings = harness_manifest.get("check_assertions", ())
    if tuple(mapping.get("check") for mapping in check_mappings) != required_host_checks:
        raise ValueError("max-test mappings do not cover the closed Max check order")
    for mapping in check_mappings:
        automation = mapping.get("automation")
        assertions = mapping.get("required_assertions")
        if automation not in {"max_test", "not_automated", "not_applicable"} or not isinstance(
            assertions, list
        ):
            raise ValueError("invalid max-test check automation category")
        if automation == "max_test":
            if not assertions or not set(assertions).issubset(produced_assertions):
                raise ValueError("automated Max check cites absent harness assertions")
        elif assertions:
            raise ValueError("non-automated Max check invents harness assertions")
    expected_assertions = {
        assertion
        for section in (artifact_mappings, check_mappings)
        for mapping in section
        for assertion in mapping.get("required_assertions", ())
    }
    if expected_assertions != produced_assertions or len(expected_assertions) != 64:
        raise ValueError("Max harness produced and consumed assertion sets diverge")
    if [
        mapping["check"] for mapping in check_mappings if mapping["automation"] == "not_applicable"
    ] != ["live_transport_discontinuities"]:
        raise ValueError("standalone max-test applicability does not match the record algebra")

    test_runs = harness_manifest.get("test_runs", ())
    if test_runs != [
        {
            "database_test_name": "sunny-runtime-smoke.maxtest",
            "patcher": expected_harness_patchers[0],
            "self_start_object": "loadbang",
            "terminator_object": "test.terminate",
        }
    ]:
        raise ValueError("max-test database identity is not bound to the staged patcher")

    objects = ("sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events")
    source_directories = (
        "sunny.lfo_tilde",
        "sunny.adsr_tilde",
        "sunny.hold_tilde",
        "sunny.clock_tilde",
        "sunny.events",
    )
    public_class_globals = {
        "sunny.lfo~": "sunny_lfo_class",
        "sunny.adsr~": "sunny_adsr_class",
        "sunny.hold~": "sunny_hold_class",
        "sunny.clock~": "sunny_clock_class",
        "sunny.events": "sunny_events_class",
    }
    required_integer_signatures = {
        "sunny.lfo~": ("sunny_lfo_seed(SunnyLfo* instance, t_atom_long seed)",),
        "sunny.clock~": ("sunny_clock_position(SunnyClock* instance, t_atom_long ticks)",),
        "sunny.events": (
            "sunny_events_event(SunnyEvents* instance,",
            "sunny_events_note(SunnyEvents* instance,",
            "sunny_events_event_after(SunnyEvents* instance,",
            "sunny_events_note_after(SunnyEvents* instance,",
        ),
    }
    package_cmake = (package / "CMakeLists.txt").read_text(encoding="utf-8")
    for object_name, source_directory in zip(objects, source_directories, strict=True):
        if f"add_subdirectory(source/{source_directory})" not in package_cmake:
            raise ValueError(f"{object_name} is not attached to the staged Max package")

        help_document = json.loads(
            (package / "help" / f"{object_name}.maxhelp").read_text(encoding="utf-8")
        )
        help_object_names = {
            box["box"].get("text", "").split()[0]
            for box in help_document["patcher"]["boxes"]
            if box["box"].get("maxclass") == "newobj" and box["box"].get("text")
        }
        if object_name not in help_object_names:
            raise ValueError(
                f"{object_name} help patcher does not instantiate the documented object"
            )

        reference = element_tree.parse(package / "docs" / f"{object_name}.maxref.xml")
        root = reference.getroot()
        if root.tag != "c74object" or root.get("name") != object_name:
            raise ValueError(f"invalid Max reference identity for {object_name}")
        documented_schemas = {
            method.get("name"): tuple(
                argument.get("type") for argument in method.findall("./arglist/arg")
            )
            for method in root.findall("./methodlist/method")
        }
        documented_optional = {
            method.get("name"): tuple(
                argument.get("optional") == "1" for argument in method.findall("./arglist/arg")
            )
            for method in root.findall("./methodlist/method")
        }
        documented_methods = set(documented_schemas)
        smoke_object_id = {
            "sunny.lfo~": "lfo",
            "sunny.adsr~": "adsr",
            "sunny.hold~": "hold",
            "sunny.clock~": "clock",
            "sunny.events": "events",
        }[object_name]
        incoming_message_ids = {
            source
            for source, _, destination, inlet in smoke_edges
            if destination == smoke_object_id
            and inlet == 0
            and smoke_objects.get(source, {}).get("maxclass") == "message"
        }
        smoke_selectors = set().union(
            *(
                _message_selectors(str(smoke_objects[source]["text"]))
                for source in incoming_message_ids
            )
        )
        if documented_methods - smoke_selectors:
            raise ValueError(
                f"{object_name} max-test patch omits selectors "
                f"{sorted(documented_methods - smoke_selectors)}"
            )
        expected_status_destination = (
            "events-status" if object_name == "sunny.events" else f"{smoke_object_id}-status"
        )
        if (smoke_object_id, 1, expected_status_destination, 0) not in smoke_edges:
            raise ValueError(f"{object_name} max-test patch does not consume machine status")
        if (
            object_name != "sunny.events"
            and (
                smoke_object_id,
                0,
                f"{smoke_object_id}-sample",
                0,
            )
            not in smoke_edges
        ):
            raise ValueError(f"{object_name} max-test patch does not consume its signal outlet")
        help_selectors: set[str] = set()
        for box in help_document["patcher"]["boxes"]:
            attributes = box["box"]
            if attributes.get("maxclass") != "message" or not attributes.get("text"):
                continue
            selector = attributes["text"].split()[0]
            try:
                float(selector)
            except ValueError:
                help_selectors.add(selector)
            else:
                help_selectors.add("float")
        undocumented_in_help = documented_methods - help_selectors
        if undocumented_in_help:
            raise ValueError(
                f"{object_name} help patcher omits methods {sorted(undocumented_in_help)}"
            )

        source_path = package / "source" / source_directory / f"{source_directory}.cpp"
        source = source_path.read_text(encoding="utf-8")
        if object_name == "sunny.clock~":
            reference_text = " ".join(root.itertext()).lower()
            help_text = " ".join(
                box["box"].get("text", "") for box in help_document["patcher"]["boxes"]
            ).lower()
            if "not a max transport" not in reference_text or "not live transport" not in help_text:
                raise ValueError("sunny.clock~ metadata lost its explicit local-clock boundary")
            for forbidden_host_path in ('gensym("transport")', "clocksource", "live.object"):
                if forbidden_host_path in source:
                    raise ValueError("sunny.clock~ acquired an unspecified Max/Live timing path")
        if object_name == "sunny.events":
            adapter_source = (repository / "src/max/itm_event_adapter.cpp").read_text(
                encoding="utf-8"
            )
            reference_text = " ".join(root.itertext()).lower()
            help_text = " ".join(
                box["box"].get("text", "") for box in help_document["patcher"]["boxes"]
            ).lower()
            required_claims = (
                "global transport",
                "overdrive",
                "scheduler in audio interrupt",
                "downstream",
            )
            if any(
                claim not in reference_text or claim not in help_text for claim in required_claims
            ):
                raise ValueError("sunny.events metadata omits a sample-accuracy precondition")
            if (
                "action invocation" not in reference_text
                or "action invocation" not in help_text
                or "host delivery" not in reference_text
                or "host delivery" not in help_text
            ):
                raise ValueError("sunny.events metadata overclaims host-effect status evidence")
            required_itm_tokens = (
                "TIME_FLAGS_PERMANENT",
                "TIME_FLAGS_LOCATION",
                "TIME_FLAGS_TRANSPORT",
                "time_new(",
                "time_setvalue(",
                "clock_fdelay(",
                "instance->itm = static_cast<t_itm*>(itm_getglobal());",
                "if (instance->itm == nullptr)",
                "itm_reference(instance->itm);",
                "itm_getticks(instance->itm)",
                "itm_dereference(instance->itm);",
                "instance->itm = nullptr;",
                "itm_getresolution(",
                "itm_getstate(instance->itm)",
                "itm_getname(instance->itm)",
            )
            if any(token not in source for token in required_itm_tokens):
                raise ValueError(
                    "sunny.events is not shaped as a global permanent ITM event source"
                )
            for lifetime_call in (
                "itm_getglobal()",
                "itm_reference(instance->itm)",
                "itm_dereference(instance->itm)",
            ):
                if source.count(lifetime_call) != 1:
                    raise ValueError("sunny.events does not own exactly one global ITM reference")
            if "adapter->fire(slot->index, events, slot->owner, cancel_slot)" not in source:
                raise ValueError(
                    "sunny.events does not stop every fired permanent slot before reuse"
                )
            required_action_guards = (
                "schedule_slot == nullptr",
                "cancel_slot == nullptr",
                "stop_slot == nullptr",
                "schedule_slot(action_context, slots[index], targets[index]);",
                "cancel_slot(action_context, slot);",
                "stop_slot(action_context, events_[begin + index].slot);",
            )
            if any(token not in adapter_source for token in required_action_guards):
                raise ValueError(
                    "ITM host-effect evidence can be published without its required action"
                )
            help_objects = {
                box["box"]["id"]: box["box"].get("text", "")
                for box in help_document["patcher"]["boxes"]
                if box["box"].get("maxclass") == "newobj"
            }
            required_help_objects = {
                "event_order": "t l l l",
                "pitch": "zl nth 1",
                "attack": "zl nth 2",
                "release": "zl nth 3",
                "attack_order": "t i i",
                "note_flag": "> 0",
                "selected_velocity": "if $i1 > 0 then $i1 else $i2",
                "xnoteout": "xnoteout 1",
            }
            if any(
                help_objects.get(identifier) != text
                for identifier, text in required_help_objects.items()
            ) or any(
                text.split()[:1] in (["noteout"], ["midiout"]) for text in help_objects.values()
            ):
                raise ValueError("sunny.events help patch drops or confuses release velocity")
            help_edges = {
                (
                    line["patchline"]["source"][0],
                    line["patchline"]["source"][1],
                    line["patchline"]["destination"][0],
                    line["patchline"]["destination"][1],
                )
                for line in help_document["patcher"]["lines"]
            }
            required_help_edges = {
                ("events", 0, "event_order", 0),
                ("event_order", 0, "pitch", 0),
                ("event_order", 1, "attack", 0),
                ("event_order", 2, "release", 0),
                ("release", 0, "selected_velocity", 1),
                ("attack", 0, "attack_order", 0),
                ("attack_order", 1, "note_flag", 0),
                ("note_flag", 0, "xnoteout", 2),
                ("attack_order", 0, "selected_velocity", 0),
                ("selected_velocity", 0, "xnoteout", 1),
                ("pitch", 0, "xnoteout", 0),
            }
            if not required_help_edges.issubset(help_edges):
                raise ValueError("sunny.events help patch lost ordered xnoteout translation")
            if "no midiout" not in help_text:
                raise ValueError("sunny.events help patch overclaims MIDI-port delivery")
            if "reserved event cells" not in help_text or "256 retained events" in help_text:
                raise ValueError("sunny.events help patch collapses reservation into retention")
            required_status_evidence = (
                "status.commands_pending",
                "status.events_reserved",
                "status.events_retained",
                "pending_commands=",
                "reserved_events=",
                "retained_events=",
            )
            if any(token not in source for token in required_status_evidence):
                raise ValueError("sunny.events status collapses producer and scheduler phases")
            required_machine_status = (
                "void* status_outlet;",
                'gensym("event_status")',
                "status.commands_enqueued != 0 ? 1 : 0",
                "status.events_scheduled != 0 ? 1 : 0",
                "status.events_fired != 0 ? 1 : 0",
                "status.callback_failures == 0 ? 1 : 0",
                "outlet_anything(instance->status_outlet",
                'gensym("transport_status")',
                "tick_after_itm(",
            )
            if any(token not in source for token in required_machine_status):
                raise ValueError("sunny.events lacks its closed machine-readable status outlet")
            if "retained=%" in source:
                raise ValueError(
                    "sunny.events status mislabels reserved capacity as retained state"
                )
            for forbidden_host_mutation in (
                "itm_pause(",
                "itm_resume(",
                "itm_seek(",
                "itm_setresolution(",
                "itm_getnamed(",
                "dsp_add64(",
            ):
                if forbidden_host_mutation in source:
                    raise ValueError(
                        "sunny.events acquired an unspecified host mutation or DSP path"
                    )
            for private_itm_api in ("itm_getsr(", "itm_gettempo("):
                if private_itm_api in source:
                    raise ValueError("sunny.events depends on a private Max ITM API")
        else:
            if "status.controls_pending" not in source or "pending=%" not in source:
                raise ValueError(f"{object_name} status omits the current control-queue gauge")
            required_dsp_lifecycle = (
                "callback_maximum_frames",
                "dsp_setup_accepted",
                "instance->dsp_setup_accepted = configured.has_value();",
                'if (!configured) report_error(instance, "dsp64", configured.error());',
                "sunny::max::detail::silence_generator_output(",
            )
            if any(token not in source for token in required_dsp_lifecycle):
                raise ValueError(
                    f"{object_name} does not close construction or rejected dsp64 setup"
                )
            if source.count('outlet_new(reinterpret_cast<t_object*>(instance), "signal")') != 1:
                raise ValueError(f"{object_name} does not create exactly one signal outlet")
            required_machine_status = (
                "void* status_outlet;",
                'gensym("host_status")',
                "status.process_calls != 0 ? 1 : 0",
                "static_cast<t_atom_long>(status.process_calls)",
                "status.process_failures == 0 ? 1 : 0",
                "t_atom evidence[8]",
                'gensym("host_status"), 8, evidence',
                "outlet_anything(instance->status_outlet",
                "host_status diagnostics from the status message",
            )
            if any(token not in source for token in required_machine_status):
                raise ValueError(f"{object_name} lacks its closed machine-readable status outlet")
            constructor = re.search(r"void\*\s+\w+_new\([^)]*\)\s*\{.*?\n\}", source, re.DOTALL)
            outlet_failure = re.search(
                r"instance->status_outlet = outlet_new\("
                r"reinterpret_cast<t_object\*>\(instance\), nullptr\);\s*"
                r"auto\* signal_outlet = outlet_new\("
                r'reinterpret_cast<t_object\*>\(instance\), "signal"\);\s*'
                r"if \(instance->status_outlet == nullptr \|\| signal_outlet == nullptr\) \{\s*"
                r"object_error\(.*?\"unable to allocate Sunny signal/status outlets\"\);\s*"
                r"object_free\(instance\);\s*return nullptr;\s*\}",
                constructor.group(0) if constructor is not None else "",
                re.DOTALL,
            )
            if outlet_failure is None:
                raise ValueError(f"{object_name} does not fail closed when outlet creation fails")
            dsp_method = re.search(r"void\s+\w+_dsp64\(.*?\n\}", source, flags=re.DOTALL)
            if (
                dsp_method is None
                or dsp_method.group(0).count("dsp_add64(") != 1
                or "return;" in dsp_method.group(0)
            ):
                raise ValueError(f"{object_name} does not register perform unconditionally")
        for required_signature in required_integer_signatures.get(object_name, ()):
            if required_signature not in source:
                raise ValueError(f"{object_name} does not preserve the SDK A_LONG atom width")
        registrations = re.findall(
            r"class_addmethod\(\s*klass,\s*reinterpret_cast<method>\(([^)]+)\),\s*"
            r'"([^"]+)",\s*([^;]+?)\);',
            source,
            flags=re.DOTALL,
        )
        guarded_method_count = len(re.findall(r"method_error\s*\|=\s*class_addmethod\(", source))
        public_class_assignment = f"{public_class_globals[object_name]} = klass;"
        required_class_lifecycle = (
            "if (klass == nullptr)",
            "t_max_err method_error = MAX_ERR_NONE;",
            "method_error != MAX_ERR_NONE",
            "class_register(CLASS_BOX, klass) != MAX_ERR_NONE",
            "(void)class_free(klass);",
            public_class_assignment,
        )
        if (
            any(token not in source for token in required_class_lifecycle)
            or guarded_method_count != len(registrations)
            or source.index("class_register(CLASS_BOX, klass) != MAX_ERR_NONE")
            > source.index(public_class_assignment)
        ):
            raise ValueError(f"{object_name} does not fail closed during class registration")
        if object_name == "sunny.events":
            required_slot_class_lifecycle = (
                "if (slot_class == nullptr) return;",
                "class_register(CLASS_NOBOX, slot_class) != MAX_ERR_NONE",
                "sunny_event_slot_class = slot_class;",
            )
            if (
                any(token not in source for token in required_slot_class_lifecycle)
                or source.count("class_register(") != 2
                or source.count("(void)class_free(slot_class);") != 2
                or source.index("class_register(CLASS_NOBOX, slot_class) != MAX_ERR_NONE")
                > source.index("sunny_event_slot_class = slot_class;")
            ):
                raise ValueError(
                    "sunny.events does not fail closed while registering its slot class"
                )
        elif source.count("class_register(") != 1 or source.count("(void)class_free(klass);") != 1:
            raise ValueError(f"{object_name} has an ambiguous public-class registration lifecycle")
        sdk_to_reference_type = {"A_FLOAT": "float", "A_LONG": "int", "A_SYM": "symbol"}
        sdk_to_cpp_type = {"A_FLOAT": "double", "A_LONG": "t_atom_long", "A_SYM": "t_symbol*"}
        checked_gimme_methods = {
            ("sunny.events", "event"): ((False, False, False, True), (3, 4)),
            ("sunny.events", "note"): ((False, False, False, False, True), (4, 5)),
            ("sunny.events", "event_after"): ((False, False, False, True), (3, 4)),
            ("sunny.events", "note_after"): ((False, False, False, False, True), (4, 5)),
        }
        registered_schemas: dict[str, tuple[str, ...]] = {}
        for function, selector, sdk_arguments in registrations:
            if selector in {"assist", "dsp64"}:
                continue
            argument_types: list[str] = []
            cpp_types: list[str] = []
            sdk_tokens = []
            for token in sdk_arguments.split(","):
                token = token.strip()
                if token == "0":
                    break
                sdk_tokens.append(token)
            if sdk_tokens == ["A_GIMME"]:
                gimme_contract = checked_gimme_methods.get((object_name, selector))
                if gimme_contract is None:
                    raise ValueError(f"{object_name} uses unchecked A_GIMME method {selector}")
                expected_optional, admitted_arities = gimme_contract
                if documented_optional[selector] != expected_optional:
                    raise ValueError(
                        f"{object_name} {selector} optional-argument metadata is not exact"
                    )
                if (
                    tuple(admitted_arities)
                    not in (
                        (3, 4),
                        (4, 5),
                    )
                    or (
                        f"argument_count != {admitted_arities[0]} && "
                        f"argument_count != {admitted_arities[1]}"
                    )
                    not in source
                ):
                    raise ValueError(f"{object_name} {selector} lacks an exact arity guard")
                if "atom_gettype(&argument) != A_LONG" not in source:
                    raise ValueError(f"{object_name} {selector} lacks an exact integer-atom guard")
                argument_types.extend(documented_schemas[selector])
                cpp_types.extend(("t_symbol*", "long", "t_atom*"))
            else:
                for token in sdk_tokens:
                    if token not in sdk_to_reference_type:
                        raise ValueError(f"{object_name} uses unchecked SDK method type {token}")
                    argument_types.append(sdk_to_reference_type[token])
                    cpp_types.append(sdk_to_cpp_type[token])
            if selector in registered_schemas:
                raise ValueError(f"{object_name} registers {selector} more than once")
            registered_schemas[selector] = tuple(argument_types)
            definition = re.search(rf"\bvoid\s+{re.escape(function)}\s*\(([^)]*)\)", source)
            if definition is None:
                raise ValueError(f"{object_name} has no inspectable definition for {function}")
            parameters = [parameter.strip() for parameter in definition.group(1).split(",")]
            if sdk_tokens == ["A_GIMME"]:
                if (
                    len(parameters) != 4
                    or parameters[1] != "t_symbol*"
                    or not parameters[2].startswith("long ")
                    or not parameters[3].startswith("t_atom* ")
                ):
                    raise ValueError(
                        f"{object_name} {selector} handler does not match Max's A_GIMME ABI"
                    )
                continue
            if len(parameters) != 1 + len(cpp_types) or any(
                not parameter.startswith(f"{expected} ")
                for parameter, expected in zip(parameters[1:], cpp_types, strict=True)
            ):
                raise ValueError(
                    f"{object_name} {selector} handler does not match its SDK argument schema"
                )
        if documented_schemas != registered_schemas:
            raise ValueError(
                f"{object_name} method metadata mismatch: documented={documented_schemas}, "
                f"registered={registered_schemas}"
            )


if __name__ == "__main__":
    main()
